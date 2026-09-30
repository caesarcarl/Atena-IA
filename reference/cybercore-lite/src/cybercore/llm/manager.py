from __future__ import annotations

import threading
import time
from contextlib import contextmanager
from dataclasses import dataclass
from typing import Iterator, Literal

from cybercore.core.resources import (
    MemorySnapshot,
    ResourceManager,
    RuntimeProfile,
    normalize_model_name,
)
from cybercore.llm.ollama import (
    OllamaClient,
    OllamaError,
)
from cybercore.llm.types import (
    InferenceOptions,
    ModelInfo,
    RunningModelInfo,
)


# ============================================================
# TIPOS
# ============================================================

PlanOrigin = Literal[
    "reused",
    "selected",
]


# ============================================================
# EXCEÇÕES
# ============================================================


class ModelManagerError(RuntimeError):
    """
    Erro genérico relacionado ao gerenciamento de modelos.
    """


class NoUsableModelError(ModelManagerError):
    """
    Nenhum dos modelos configurados pôde ser usado.
    """


class ConfiguredModelNotInstalledError(ModelManagerError):
    """
    Nenhum modelo compatível com os perfis do CyberCore
    está instalado localmente.
    """


# ============================================================
# PLANO DE EXECUÇÃO
# ============================================================


@dataclass(slots=True, frozen=True)
class ModelPlan:
    """
    Resultado da decisão tomada pelo ModelManager.

    O plano informa:

        qual perfil usar
        qual modelo usar
        quais opções de inferência usar
        se o modelo já estava carregado
        quanta memória havia no momento da decisão

    O ModelPlan não executa a inferência.

    Ele apenas descreve como ela deverá ser executada.
    """

    profile: RuntimeProfile

    options: InferenceOptions

    origin: PlanOrigin

    available_memory_mib: int

    running_before: tuple[str, ...] = ()

    @property
    def model(self) -> str:
        return self.profile.model

    @property
    def reused_loaded_model(self) -> bool:
        return self.origin == "reused"

    def to_dict(self) -> dict:
        return {
            "profile": self.profile.name,
            "model": self.model,
            "origin": self.origin,

            "reused_loaded_model":
                self.reused_loaded_model,

            "available_memory_mib":
                self.available_memory_mib,

            "running_before":
                list(self.running_before),

            "options":
                self.options.to_dict(),
        }


# ============================================================
# STATUS
# ============================================================


@dataclass(slots=True, frozen=True)
class ModelManagerStatus:
    """
    Fotografia do estado dos modelos vistos pelo CyberCore.
    """

    installed: tuple[ModelInfo, ...]

    running: tuple[RunningModelInfo, ...]

    active_profile: RuntimeProfile | None

    memory: MemorySnapshot

    @property
    def installed_names(self) -> tuple[str, ...]:
        return tuple(
            model.name
            for model in self.installed
        )

    @property
    def running_names(self) -> tuple[str, ...]:
        return tuple(
            model.name
            for model in self.running
        )

    def to_dict(self) -> dict:
        return {
            "installed": [
                model.to_dict()
                for model in self.installed
            ],

            "running": [
                model.to_dict()
                for model in self.running
            ],

            "active_profile": (
                self.active_profile.to_dict()
                if self.active_profile
                else None
            ),

            "memory":
                self.memory.to_dict(),
        }


# ============================================================
# MODEL MANAGER
# ============================================================


class ModelManager:
    """
    Gerencia o ciclo de vida dos modelos usados pelo CyberCore.

    Responsabilidades:

        - descobrir modelos instalados
        - descobrir modelos carregados
        - reconhecer perfis do CyberCore
        - reaproveitar modelos já residentes
        - impedir múltiplos LLMs residentes
        - escolher modelo de acordo com RAM
        - converter RuntimeProfile para InferenceOptions
        - descarregar modelos quando necessário

    Ele NÃO:

        - envia prompts
        - gera texto
        - implementa RAG
        - conversa com o usuário

    Essas responsabilidades pertencem a outras camadas.
    """

    def __init__(
        self,
        ollama: OllamaClient,
        resources: ResourceManager | None = None,
        *,
        settle_seconds: float = 0.25,
    ) -> None:

        self.ollama = ollama

        self.resources = (
            resources
            if resources is not None
            else ResourceManager()
        )

        self.settle_seconds = max(
            0.0,
            settle_seconds,
        )

        #
        # RLock:
        #
        # Apenas uma operação de gerenciamento/inferência
        # deverá alterar o estado dos modelos por vez.
        #
        # RLock é reentrante:
        # o mesmo thread pode adquirir o lock novamente.
        #
        self._lock = threading.RLock()

    # ========================================================
    # LOCK DE INFERÊNCIA
    # ========================================================

    @contextmanager
    def inference_guard(
        self,
    ) -> Iterator[None]:
        """
        Cria uma seção crítica para uma inferência.

        Futuramente o Engine poderá fazer:

            with manager.inference_guard():
                plan = manager.prepare()
                ollama.stream_chat(...)

        Isso impede que outra requisição troque o modelo
        enquanto a primeira ainda está sendo executada.

        No nosso CLI atual existe apenas uma inferência por vez,
        mas já deixamos a arquitetura preparada para uma API
        ou interface gráfica futura.
        """

        with self._lock:
            yield

    # ========================================================
    # MODELOS INSTALADOS
    # ========================================================

    def installed_models(
        self,
    ) -> tuple[ModelInfo, ...]:
        """
        Retorna modelos instalados no Ollama.
        """

        return tuple(
            self.ollama.list_models()
        )

    def installed_model_names(
        self,
    ) -> set[str]:
        """
        Retorna nomes normalizados dos modelos instalados.
        """

        return {
            normalize_model_name(
                model.name
            )
            for model in self.installed_models()
        }

    def is_installed(
        self,
        model: str,
    ) -> bool:
        """
        Verifica se um modelo está instalado.
        """

        wanted = normalize_model_name(
            model
        )

        return (
            wanted
            in self.installed_model_names()
        )

    # ========================================================
    # MODELOS RESIDENTES
    # ========================================================

    def running_models(
        self,
    ) -> tuple[RunningModelInfo, ...]:
        """
        Retorna os modelos atualmente residentes no Ollama.
        """

        return tuple(
            self.ollama.running_models()
        )

    def running_model_names(
        self,
    ) -> tuple[str, ...]:
        """
        Retorna somente os nomes dos modelos residentes.
        """

        return tuple(
            model.name
            for model in self.running_models()
        )

    # ========================================================
    # PERFIL DO MODELO RESIDENTE
    # ========================================================

    def profile_for_running_model(
        self,
    ) -> RuntimeProfile | None:
        """
        Procura um modelo residente que corresponda
        a algum perfil configurado no CyberCore.

        Se múltiplos modelos estiverem residentes,
        respeitamos a ordem de prioridade definida
        em ResourceManager.profiles.
        """

        running = self.running_models()

        if not running:
            return None

        running_names = {
            normalize_model_name(
                model.name
            )
            for model in running
        }

        for profile in self.resources.profiles:

            profile_name = normalize_model_name(
                profile.model
            )

            if profile_name in running_names:
                return profile

        return None

    # ========================================================
    # STATUS
    # ========================================================

    def status(
        self,
    ) -> ModelManagerStatus:
        """
        Retorna uma visão consolidada do estado atual.
        """

        installed = self.installed_models()

        running = self.running_models()

        active_profile = None

        running_names = {
            normalize_model_name(
                model.name
            )
            for model in running
        }

        for profile in self.resources.profiles:

            if (
                normalize_model_name(
                    profile.model
                )
                in running_names
            ):
                active_profile = profile
                break

        return ModelManagerStatus(
            installed=installed,

            running=running,

            active_profile=active_profile,

            memory=self.resources.memory(),
        )

    # ========================================================
    # OPÇÕES DE INFERÊNCIA
    # ========================================================

    def options_for_profile(
        self,
        profile: RuntimeProfile,
    ) -> InferenceOptions:
        """
        Converte um RuntimeProfile do CyberCore
        em InferenceOptions.

        ResourceManager decide:

            "qual perfil cabe?"

        ModelManager transforma essa decisão em:

            "como o LLM será executado?"
        """

        cpu = self.resources.cpu()

        threads = profile.recommended_threads(
            cpu.logical_cpus
        )

        return InferenceOptions(
            context_length=(
                profile.context_length
            ),

            max_output_tokens=(
                profile.max_output_tokens
            ),

            num_threads=threads,

            temperature=(
                profile.temperature
            ),

            top_p=(
                profile.top_p
            ),

            top_k=(
                profile.top_k
            ),

            keep_alive=(
                profile.keep_alive
            ),

            think=(
                profile.think
            ),
        )

    # ========================================================
    # SELEÇÃO DE NOVO PERFIL
    # ========================================================

    def _choose_installed_profile(
        self,
    ) -> RuntimeProfile:
        """
        Escolhe o melhor perfil que:

            1. está instalado
            2. cabe na memória atual

        A ordem de ResourceManager.profiles define
        prioridade.

        Isso corrige uma situação importante:

            Gemma cabe na RAM,
            mas Gemma não está instalado.

        Nesse caso não devemos falhar imediatamente.
        Tentamos o próximo perfil disponível.
        """

        memory = self.resources.memory()

        installed = self.installed_model_names()

        compatible_installed = False

        for profile in self.resources.profiles:

            model_name = normalize_model_name(
                profile.model
            )

            if model_name not in installed:
                continue

            compatible_installed = True

            if (
                memory.available_mib
                >= profile.min_available_to_load_mib
            ):
                return profile

        if not compatible_installed:
            configured = ", ".join(
                profile.model
                for profile
                in self.resources.profiles
            )

            raise ConfiguredModelNotInstalledError(
                "Nenhum modelo configurado "
                "nos perfis do CyberCore está instalado. "
                f"Perfis esperados: {configured}"
            )

        raise NoUsableModelError(
            "Existem modelos compatíveis instalados, "
            "mas nenhum possui margem de RAM suficiente "
            "neste momento. "
            f"MemAvailable={memory.available_mib} MiB."
        )

    # ========================================================
    # REUTILIZAÇÃO
    # ========================================================

    def _try_reuse_running_profile(
        self,
    ) -> RuntimeProfile | None:
        """
        Tenta reutilizar um modelo já carregado.

        Esta função implementa uma distinção fundamental:

            RAM necessária PARA CARREGAR
            !=
            RAM necessária PARA CONTINUAR USANDO.

        Depois que um modelo entra na RAM,
        MemAvailable naturalmente diminui.

        Não podemos interpretar essa queda como se
        o modelo ainda precisasse ser carregado.
        """

        profile = (
            self.profile_for_running_model()
        )

        if profile is None:
            return None

        if self.resources.can_keep_loaded(
            profile
        ):
            return profile

        return None

    # ========================================================
    # DESCARREGAMENTO
    # ========================================================

    def _wait_for_memory_accounting(
        self,
    ) -> None:
        """
        Pequena pausa para permitir que o kernel e o Ollama
        atualizem o estado depois de um descarregamento.

        Não é garantia de que a memória será imediatamente
        reorganizada; serve apenas para evitar medir o estado
        literalmente no mesmo instante da solicitação.
        """

        if self.settle_seconds <= 0:
            return

        time.sleep(
            self.settle_seconds
        )

    def unload_all(
        self,
    ) -> None:
        """
        Descarrega todos os modelos residentes.
        """

        with self._lock:
            self.ollama.unload_all()

            self._wait_for_memory_accounting()

    def unload_model(
        self,
        model: str,
    ) -> None:
        """
        Descarrega um modelo específico.
        """

        with self._lock:
            self.ollama.unload_model(
                model
            )

            self._wait_for_memory_accounting()

    # ========================================================
    # PREPARAÇÃO
    # ========================================================

    def prepare(
        self,
    ) -> ModelPlan:
        """
        Prepara a próxima inferência.

        Estratégia:

        1. verifica quais modelos já estão residentes

        2. tenta reaproveitar um modelo pertencente
           aos perfis do CyberCore

        3. se esse modelo ainda possui margem segura,
           mantém esse modelo

        4. descarrega qualquer outro modelo residente

        5. se não houver modelo reaproveitável:
           descarrega modelos antigos/externos

        6. mede novamente a RAM

        7. escolhe o melhor perfil instalado que cabe

        IMPORTANTE:

        prepare() NÃO carrega explicitamente o novo modelo.

        O modelo será carregado naturalmente quando
        OllamaClient.chat() ou stream_chat() for chamado.

        Isso evita:

            load_model()
            +
            chat()

        causando duas requisições sequenciais desnecessárias.
        """

        with self._lock:

            running_before = (
                self.running_model_names()
            )

            # ------------------------------------------------
            # 1. TENTAR REUTILIZAÇÃO
            # ------------------------------------------------

            reusable = (
                self._try_reuse_running_profile()
            )

            if reusable is not None:

                #
                # Mesmo que exista o modelo correto,
                # pode existir outro modelo junto.
                #
                # Nossa política para máquinas pequenas é:
                #
                #     um LLM residente por vez.
                #
                self.ollama.unload_all_except(
                    reusable.model
                )

                memory = (
                    self.resources.memory()
                )

                options = (
                    self.options_for_profile(
                        reusable
                    )
                )

                return ModelPlan(
                    profile=reusable,

                    options=options,

                    origin="reused",

                    available_memory_mib=(
                        memory.available_mib
                    ),

                    running_before=(
                        running_before
                    ),
                )

            # ------------------------------------------------
            # 2. NÃO É POSSÍVEL REUTILIZAR
            # ------------------------------------------------

            #
            # Pode haver:
            #
            # - modelo desconhecido
            # - outro modelo
            # - modelo conhecido sem margem segura
            #
            if running_before:

                self.ollama.unload_all()

                self._wait_for_memory_accounting()

            # ------------------------------------------------
            # 3. ESCOLHER NOVO PERFIL
            # ------------------------------------------------

            profile = (
                self._choose_installed_profile()
            )

            #
            # Segurança adicional:
            # nenhum outro modelo deve permanecer.
            #
            self.ollama.unload_all_except(
                profile.model
            )

            memory = (
                self.resources.memory()
            )

            options = (
                self.options_for_profile(
                    profile
                )
            )

            return ModelPlan(
                profile=profile,

                options=options,

                origin="selected",

                available_memory_mib=(
                    memory.available_mib
                ),

                running_before=(
                    running_before
                ),
            )

    # ========================================================
    # REPROFILE
    # ========================================================

    def reprofile(
        self,
    ) -> ModelPlan:
        """
        Força uma nova seleção.

        Útil quando:

            o usuário fechou programas
            a RAM disponível aumentou
            queremos tentar um modelo maior
        """

        with self._lock:

            self.ollama.unload_all()

            self._wait_for_memory_accounting()

            return self.prepare()

    # ========================================================
    # WARM LOAD
    # ========================================================

    def warm(
        self,
    ) -> ModelPlan:
        """
        Seleciona um perfil e carrega explicitamente
        o modelo na memória.

        Não deve ser necessário no fluxo normal.

        Será útil posteriormente para:

            benchmark
            pré-aquecimento
            servidor/API
        """

        with self._lock:

            plan = self.prepare()

            if not self.ollama.is_model_running(
                plan.model
            ):
                self.ollama.load_model(
                    plan.model,

                    keep_alive=(
                        plan.options.keep_alive
                    ),
                )

            return plan
