from __future__ import annotations

from collections.abc import Iterator, Sequence
from dataclasses import dataclass

from cybercore.core.prompts import SYSTEM_PROMPT
from cybercore.core.resources import (
    CpuSnapshot,
    MemorySnapshot,
    ResourceManager,
    RuntimeProfile,
)
from cybercore.llm.manager import (
    ModelManager,
    ModelManagerStatus,
    ModelPlan,
)
from cybercore.llm.ollama import (
    OllamaClient,
    OllamaError,
    OllamaUnavailable,
)
from cybercore.llm.types import (
    ChatMessage,
    ChatResult,
    ModelInfo,
    RunningModelInfo,
    StreamChunk,
)


# ============================================================
# EXCEÇÕES
# ============================================================


class CyberCoreError(RuntimeError):
    """
    Erro genérico da camada de orquestração do CyberCore.
    """


class InvalidPromptError(CyberCoreError):
    """
    A solicitação enviada ao Core é inválida.
    """


# ============================================================
# SOLICITAÇÃO DE INFERÊNCIA
# ============================================================


@dataclass(slots=True, frozen=True)
class InferenceRequest:
    """
    Representa uma solicitação entregue ao Engine.

    O Engine recebe:

        prompt
        histórico opcional
        contexto externo opcional

    Futuramente o campo context poderá receber:

        RAG
        pesquisa web
        resultado de ferramentas
        base CVE
        memória

    sem que seja necessário alterar a interface fundamental
    do Engine.
    """

    prompt: str

    history: tuple[ChatMessage, ...] = ()

    context: str | None = None

    def __post_init__(self) -> None:

        if not self.prompt.strip():
            raise InvalidPromptError(
                "O prompt não pode estar vazio."
            )


# ============================================================
# STATUS DO CORE
# ============================================================


@dataclass(slots=True, frozen=True)
class CoreStatus:
    """
    Estado geral do CyberCore.

    Esse status contém dados reais do sistema e do Ollama.

    Ele NÃO é gerado pelo modelo de linguagem.
    """

    ollama_version: str

    memory: MemorySnapshot

    cpu: CpuSnapshot

    installed_models: tuple[
        ModelInfo,
        ...,
    ]

    running_models: tuple[
        RunningModelInfo,
        ...,
    ]

    active_profile: RuntimeProfile | None

    @property
    def running_model_names(
        self,
    ) -> tuple[str, ...]:

        return tuple(
            model.name
            for model in self.running_models
        )

    @property
    def installed_model_names(
        self,
    ) -> tuple[str, ...]:

        return tuple(
            model.name
            for model in self.installed_models
        )

    def to_dict(self) -> dict:

        return {
            "ollama_version":
                self.ollama_version,

            "memory":
                self.memory.to_dict(),

            "cpu":
                self.cpu.to_dict(),

            "installed_models": [
                model.to_dict()
                for model
                in self.installed_models
            ],

            "running_models": [
                model.to_dict()
                for model
                in self.running_models
            ],

            "active_profile": (
                self.active_profile.to_dict(
                    logical_cpus=(
                        self.cpu.logical_cpus
                    )
                )
                if self.active_profile
                else None
            ),
        }


# ============================================================
# CYBERCORE ENGINE
# ============================================================


class CyberCoreEngine:
    """
    Orquestrador principal do CyberCore Lite.

    Fluxo básico:

        usuário
           ↓
        Engine
           ↓
        ModelManager
           ↓
        ResourceManager
           ↓
        OllamaClient
           ↓
        Ollama
           ↓
        modelo

    O Engine NÃO deve implementar diretamente:

        HTTP
        leitura de /proc
        gerenciamento de modelos
        parser de PDF
        banco de dados
        pesquisa web

    Ele apenas coordena esses subsistemas.
    """

    def __init__(
        self,
        ollama: OllamaClient | None = None,
        resources: ResourceManager | None = None,
        manager: ModelManager | None = None,
        *,
        system_prompt: str | None = None,
    ) -> None:

        # ----------------------------------------------------
        # OLLAMA
        # ----------------------------------------------------

        self.ollama = (
            ollama
            if ollama is not None
            else OllamaClient()
        )

        # ----------------------------------------------------
        # RECURSOS
        # ----------------------------------------------------

        self.resources = (
            resources
            if resources is not None
            else ResourceManager()
        )

        # ----------------------------------------------------
        # MODEL MANAGER
        # ----------------------------------------------------

        if manager is not None:

            self.models = manager

        else:

            self.models = ModelManager(
                ollama=self.ollama,
                resources=self.resources,
            )

        # ----------------------------------------------------
        # IDENTIDADE / SYSTEM PROMPT
        # ----------------------------------------------------

        #
        # Se nenhuma identidade for passada diretamente,
        # usamos o SYSTEM_PROMPT definido em prompts.py.
        #
        # Isso permite que você personalize Tartarus/CyberCore
        # sem alterar o Engine.
        #

        self.system_prompt = (
            system_prompt.strip()
            if system_prompt is not None
            else SYSTEM_PROMPT.strip()
        )

        if not self.system_prompt:
            raise ValueError(
                "system_prompt não pode estar vazio."
            )

    # ========================================================
    # STATUS
    # ========================================================

    def status(
        self,
    ) -> CoreStatus:
        """
        Obtém o estado atual do CyberCore.

        Importante:

        este método não executa inferência e não precisa
        perguntar nada ao LLM.
        """

        manager_status = (
            self.models.status()
        )

        cpu = (
            self.resources.cpu()
        )

        return CoreStatus(
            ollama_version=(
                self.ollama.version()
            ),

            memory=(
                manager_status.memory
            ),

            cpu=cpu,

            installed_models=(
                manager_status.installed
            ),

            running_models=(
                manager_status.running
            ),

            active_profile=(
                manager_status.active_profile
            ),
        )

    # ========================================================
    # CONSTRUÇÃO DAS MENSAGENS
    # ========================================================

    def build_messages(
        self,
        request: InferenceRequest,
    ) -> list[ChatMessage]:
        """
        Monta as mensagens que serão enviadas ao LLM.

        Ordem:

            1. system prompt
            2. contexto externo, se houver
            3. histórico
            4. pergunta atual

        Essa ordem será importante quando adicionarmos RAG.
        """

        messages: list[
            ChatMessage
        ] = []

        # ----------------------------------------------------
        # SYSTEM
        # ----------------------------------------------------

        messages.append(
            ChatMessage(
                role="system",
                content=self.system_prompt,
            )
        )

        # ----------------------------------------------------
        # CONTEXTO EXTERNO
        # ----------------------------------------------------

        if (
            request.context is not None
            and request.context.strip()
        ):

            context_text = (
                request.context.strip()
            )

            messages.append(
                ChatMessage(
                    role="system",
                    content=(
                        "Contexto técnico fornecido "
                        "ao CyberCore:\n\n"
                        f"{context_text}"
                    ),
                )
            )

        # ----------------------------------------------------
        # HISTÓRICO
        # ----------------------------------------------------

        messages.extend(
            request.history
        )

        # ----------------------------------------------------
        # PERGUNTA ATUAL
        # ----------------------------------------------------

        messages.append(
            ChatMessage(
                role="user",
                content=request.prompt.strip(),
            )
        )

        return messages

    # ========================================================
    # PREPARAÇÃO
    # ========================================================

    def prepare(
        self,
    ) -> ModelPlan:
        """
        Solicita ao ModelManager um plano de execução.

        Não executa a inferência.
        """

        return self.models.prepare()

    # ========================================================
    # CHAT SEM STREAMING
    # ========================================================

    def ask(
        self,
        prompt: str,
        *,
        history: Sequence[
            ChatMessage
        ] = (),
        context: str | None = None,
    ) -> ChatResult:
        """
        Executa uma inferência completa sem streaming.
        """

        request = InferenceRequest(
            prompt=prompt,

            history=tuple(
                history
            ),

            context=context,
        )

        messages = (
            self.build_messages(
                request
            )
        )

        #
        # inference_guard mantém o modelo estável
        # durante toda a inferência.
        #
        # Outra thread não poderá trocar o modelo
        # enquanto esta chamada ainda estiver ativa.
        #

        with self.models.inference_guard():

            plan = (
                self.models.prepare()
            )

            return self.ollama.chat(
                model=plan.model,

                messages=messages,

                options=plan.options,
            )

    # ========================================================
    # CHAT COM STREAMING
    # ========================================================

    def stream(
        self,
        prompt: str,
        *,
        history: Sequence[
            ChatMessage
        ] = (),
        context: str | None = None,
    ) -> Iterator[StreamChunk]:
        """
        Executa inferência com streaming.

        O lock permanece ativo durante toda a geração.

        Isso impede que outro fluxo descarregue/troque
        o modelo enquanto o Ollama ainda está produzindo
        tokens.
        """

        request = InferenceRequest(
            prompt=prompt,

            history=tuple(
                history
            ),

            context=context,
        )

        messages = (
            self.build_messages(
                request
            )
        )

        with self.models.inference_guard():

            plan = (
                self.models.prepare()
            )

            yield from (
                self.ollama.stream_chat(
                    model=plan.model,

                    messages=messages,

                    options=plan.options,
                )
            )

    # ========================================================
    # WARM
    # ========================================================

    def warm(
        self,
    ) -> ModelPlan:
        """
        Pré-carrega o modelo selecionado.

        Útil futuramente para:

            benchmark
            API
            GUI
            inicialização controlada
        """

        return self.models.warm()

    # ========================================================
    # UNLOAD
    # ========================================================

    def unload(
        self,
    ) -> None:
        """
        Descarrega todos os modelos do CyberCore.
        """

        self.models.unload_all()

    # ========================================================
    # REPROFILE
    # ========================================================

    def reprofile(
        self,
    ) -> ModelPlan:
        """
        Força o CyberCore a:

            descarregar modelos
            medir RAM novamente
            escolher novo perfil

        Útil depois que o usuário fecha programas
        e libera memória.
        """

        return self.models.reprofile()

    # ========================================================
    # MODELOS
    # ========================================================

    def installed_models(
        self,
    ) -> tuple[
        ModelInfo,
        ...,
    ]:
        """
        Retorna modelos instalados.
        """

        return (
            self.models.installed_models()
        )

    def running_models(
        self,
    ) -> tuple[
        RunningModelInfo,
        ...,
    ]:
        """
        Retorna modelos residentes na memória.
        """

        return (
            self.models.running_models()
        )

    # ========================================================
    # SAÚDE
    # ========================================================

    def is_ollama_available(
        self,
    ) -> bool:
        """
        Verifica se o servidor Ollama está acessível.
        """

        return self.ollama.ping()
