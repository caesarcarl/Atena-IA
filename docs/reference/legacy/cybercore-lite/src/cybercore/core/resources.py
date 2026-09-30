from __future__ import annotations

import os
import platform
from dataclasses import dataclass
from pathlib import Path
from typing import Literal


# ============================================================
# CONSTANTES
# ============================================================

PROC_MEMINFO = Path("/proc/meminfo")
PROC_LOADAVG = Path("/proc/loadavg")

KIB_PER_MIB = 1024


MemoryPressure = Literal[
    "comfortable",
    "moderate",
    "tight",
    "critical",
]


# ============================================================
# EXCEÇÕES
# ============================================================


class ResourceError(RuntimeError):
    """
    Erro genérico relacionado à leitura ou análise
    de recursos da máquina.
    """


class UnsupportedPlatformError(ResourceError):
    """
    O sistema operacional não possui os mecanismos
    necessários para esta implementação.
    """


# ============================================================
# SNAPSHOT DE MEMÓRIA
# ============================================================


@dataclass(slots=True, frozen=True)
class MemorySnapshot:
    """
    Fotografia do estado da memória em um determinado
    instante.

    Todos os valores são expressos em MiB.

    MiB:
        Mebibyte.

        1 MiB = 1024 × 1024 bytes.

    Usamos MiB porque /proc/meminfo trabalha com
    unidades binárias.
    """

    total_mib: int

    available_mib: int

    free_mib: int

    cached_mib: int

    buffers_mib: int

    swap_total_mib: int

    swap_free_mib: int

    swap_used_mib: int

    # --------------------------------------------------------
    # MÉTRICAS DERIVADAS
    # --------------------------------------------------------

    @property
    def used_mib(self) -> int:
        """
        Estimativa de memória não imediatamente disponível.

        Não deve ser interpretada como:

            total - free

        porque Linux utiliza RAM livre para cache.
        """

        return max(
            0,
            self.total_mib
            - self.available_mib,
        )

    @property
    def available_ratio(self) -> float:
        """
        Fração da RAM total que ainda está disponível.

        Exemplo:

            0.40 = 40%
        """

        if self.total_mib <= 0:
            return 0.0

        return (
            self.available_mib
            / self.total_mib
        )

    @property
    def available_percent(self) -> float:
        return (
            self.available_ratio
            * 100.0
        )

    @property
    def swap_used_ratio(self) -> float:
        """
        Percentual lógico de swap ocupada.

        Importante:

        swap ocupada não significa necessariamente
        swap sendo acessada naquele momento.
        """

        if self.swap_total_mib <= 0:
            return 0.0

        return (
            self.swap_used_mib
            / self.swap_total_mib
        )

    @property
    def swap_used_percent(self) -> float:
        return (
            self.swap_used_ratio
            * 100.0
        )

    @property
    def pressure(self) -> MemoryPressure:
        """
        Classificação simples da pressão de memória.

        Ela não decide sozinha qual modelo usar.

        Serve principalmente para:

            status
            logs
            interface
            diagnósticos
        """

        available = self.available_mib

        if available >= 1800:
            return "comfortable"

        if available >= 1200:
            return "moderate"

        if available >= 750:
            return "tight"

        return "critical"

    def to_dict(self) -> dict:
        return {
            "total_mib": self.total_mib,

            "available_mib":
                self.available_mib,

            "free_mib":
                self.free_mib,

            "used_mib":
                self.used_mib,

            "cached_mib":
                self.cached_mib,

            "buffers_mib":
                self.buffers_mib,

            "swap_total_mib":
                self.swap_total_mib,

            "swap_free_mib":
                self.swap_free_mib,

            "swap_used_mib":
                self.swap_used_mib,

            "available_percent":
                self.available_percent,

            "swap_used_percent":
                self.swap_used_percent,

            "pressure":
                self.pressure,
        }


# ============================================================
# SNAPSHOT DE CPU
# ============================================================


@dataclass(slots=True, frozen=True)
class CpuSnapshot:
    """
    Informações simples sobre a CPU e carga do sistema.

    Não estamos tentando substituir ferramentas como
    top, htop ou psutil.

    Queremos somente as informações necessárias para
    decisões do CyberCore.
    """

    architecture: str

    logical_cpus: int

    load_1m: float

    load_5m: float

    load_15m: float

    @property
    def load_per_cpu_1m(self) -> float:
        """
        Normaliza load average pelo número de CPUs lógicas.

        Exemplo:

            load = 2
            CPUs = 2

            load_per_cpu = 1.0
        """

        if self.logical_cpus <= 0:
            return 0.0

        return (
            self.load_1m
            / self.logical_cpus
        )

    def to_dict(self) -> dict:
        return {
            "architecture":
                self.architecture,

            "logical_cpus":
                self.logical_cpus,

            "load_1m":
                self.load_1m,

            "load_5m":
                self.load_5m,

            "load_15m":
                self.load_15m,

            "load_per_cpu_1m":
                self.load_per_cpu_1m,
        }


# ============================================================
# SNAPSHOT COMPLETO
# ============================================================


@dataclass(slots=True, frozen=True)
class ResourceSnapshot:
    """
    Visão agregada dos recursos relevantes
    para uma inferência local.
    """

    memory: MemorySnapshot

    cpu: CpuSnapshot

    def to_dict(self) -> dict:
        return {
            "memory":
                self.memory.to_dict(),

            "cpu":
                self.cpu.to_dict(),
        }


# ============================================================
# PERFIL DE EXECUÇÃO
# ============================================================


@dataclass(slots=True, frozen=True)
class RuntimeProfile:
    """
    Define uma estratégia de inferência adequada
    a determinada quantidade de recursos.

    Este objeto não pertence ao Ollama.

    Ele pertence ao CyberCore.

    Mais tarde o Model Manager converterá este perfil
    para InferenceOptions.
    """

    name: str

    model: str

    context_length: int

    max_output_tokens: int

    keep_alive: str | int

    min_available_to_load_mib: int

    min_available_while_loaded_mib: int

    max_threads: int = 4

    temperature: float = 0.2

    top_p: float = 0.9

    top_k: int = 40

    think: bool = False

    def __post_init__(self) -> None:
        if not self.name:
            raise ValueError(
                "RuntimeProfile precisa de um nome"
            )

        if not self.model:
            raise ValueError(
                "RuntimeProfile precisa de um modelo"
            )

        if self.context_length <= 0:
            raise ValueError(
                "context_length deve ser maior que zero"
            )

        if self.max_output_tokens <= 0:
            raise ValueError(
                "max_output_tokens deve ser maior que zero"
            )

        if self.min_available_to_load_mib <= 0:
            raise ValueError(
                "min_available_to_load_mib "
                "deve ser maior que zero"
            )

        if (
            self.min_available_while_loaded_mib
            <= 0
        ):
            raise ValueError(
                "min_available_while_loaded_mib "
                "deve ser maior que zero"
            )

        if self.max_threads <= 0:
            raise ValueError(
                "max_threads deve ser maior que zero"
            )

    def recommended_threads(
        self,
        logical_cpus: int,
    ) -> int:
        """
        Escolhe quantas threads o perfil poderá usar.

        No seu N4020:

            logical_cpus = 2

        portanto:

            min(2, max_threads)

        resultará normalmente em 2.
        """

        if logical_cpus <= 0:
            return 1

        return max(
            1,
            min(
                logical_cpus,
                self.max_threads,
            ),
        )

    def to_dict(
        self,
        *,
        logical_cpus: int | None = None,
    ) -> dict:
        data = {
            "name":
                self.name,

            "model":
                self.model,

            "context_length":
                self.context_length,

            "max_output_tokens":
                self.max_output_tokens,

            "keep_alive":
                self.keep_alive,

            "min_available_to_load_mib":
                self.min_available_to_load_mib,

            "min_available_while_loaded_mib":
                self.min_available_while_loaded_mib,

            "max_threads":
                self.max_threads,

            "temperature":
                self.temperature,

            "top_p":
                self.top_p,

            "top_k":
                self.top_k,

            "think":
                self.think,
        }

        if logical_cpus is not None:
            data["recommended_threads"] = (
                self.recommended_threads(
                    logical_cpus
                )
            )

        return data


# ============================================================
# PERFIS PADRÃO
# ============================================================

#
# IMPORTANTE:
#
# Estes valores são nossa primeira política operacional.
#
# Eles NÃO significam:
#
#     "Gemma exige exatamente 1600 MiB"
#
# Significam:
#
#     "Só queremos tentar carregar Gemma quando houver
#      aproximadamente essa margem."
#
# Depois os benchmarks reais substituirão essas
# estimativas por valores medidos.
#


DEFAULT_PROFILES: tuple[
    RuntimeProfile,
    ...,
] = (

    # --------------------------------------------------------
    # PERFIL 1B
    # --------------------------------------------------------

    RuntimeProfile(
        name="balanced-1b",

        model="gemma3:1b",

        context_length=1024,

        max_output_tokens=256,

        keep_alive="45s",

        min_available_to_load_mib=1600,

        min_available_while_loaded_mib=500,

        max_threads=2,

        temperature=0.2,

        top_p=0.9,

        top_k=40,

        think=False,
    ),

    # --------------------------------------------------------
    # PERFIL LEVE
    # --------------------------------------------------------

    RuntimeProfile(
        name="light-0.6b",

        model="qwen3:0.6b",

        context_length=1024,

        max_output_tokens=256,

        keep_alive="60s",

        min_available_to_load_mib=850,

        min_available_while_loaded_mib=350,

        max_threads=2,

        temperature=0.2,

        top_p=0.9,

        top_k=40,

        think=False,
    ),

    # --------------------------------------------------------
    # PERFIL DE EMERGÊNCIA
    # --------------------------------------------------------

    RuntimeProfile(
        name="emergency-0.5b",

        model="qwen2.5:0.5b",

        context_length=768,

        max_output_tokens=192,

        keep_alive="20s",

        min_available_to_load_mib=600,

        min_available_while_loaded_mib=250,

        max_threads=2,

        temperature=0.2,

        top_p=0.9,

        top_k=40,

        think=False,
    ),
)


# ============================================================
# LEITURA DO /PROC
# ============================================================


def _ensure_linux_proc() -> None:
    """
    Garante que estamos em um ambiente Linux
    com /proc disponível.
    """

    if not PROC_MEMINFO.exists():
        raise UnsupportedPlatformError(
            "O CyberCore Resource Manager "
            "não encontrou /proc/meminfo."
        )


def _read_meminfo_kib() -> dict[str, int]:
    """
    Lê /proc/meminfo.

    O Linux fornece valores em KiB.

    Exemplo:

        MemTotal:       3525536 kB

    Internamente retornamos:

        {
            "MemTotal": 3525536
        }
    """

    _ensure_linux_proc()

    values: dict[str, int] = {}

    try:
        with PROC_MEMINFO.open(
            "r",
            encoding="utf-8",
        ) as file:

            for line in file:

                if ":" not in line:
                    continue

                key, raw_value = (
                    line.split(
                        ":",
                        1,
                    )
                )

                parts = (
                    raw_value
                    .strip()
                    .split()
                )

                if not parts:
                    continue

                try:
                    values[key] = int(
                        parts[0]
                    )

                except ValueError:
                    continue

    except OSError as exc:
        raise ResourceError(
            "Não foi possível ler "
            "/proc/meminfo."
        ) from exc

    return values


def _kib_to_mib(
    value_kib: int,
) -> int:
    """
    Converte KiB para MiB.
    """

    return (
        value_kib
        // KIB_PER_MIB
    )


# ============================================================
# MEMÓRIA
# ============================================================


def memory_snapshot() -> MemorySnapshot:
    """
    Obtém o estado atual da memória.
    """

    info = _read_meminfo_kib()

    total = _kib_to_mib(
        info.get(
            "MemTotal",
            0,
        )
    )

    available = _kib_to_mib(
        info.get(
            "MemAvailable",
            0,
        )
    )

    free = _kib_to_mib(
        info.get(
            "MemFree",
            0,
        )
    )

    cached = _kib_to_mib(
        info.get(
            "Cached",
            0,
        )
        + info.get(
            "SReclaimable",
            0,
        )
    )

    buffers = _kib_to_mib(
        info.get(
            "Buffers",
            0,
        )
    )

    swap_total = _kib_to_mib(
        info.get(
            "SwapTotal",
            0,
        )
    )

    swap_free = _kib_to_mib(
        info.get(
            "SwapFree",
            0,
        )
    )

    swap_used = max(
        0,
        swap_total
        - swap_free,
    )

    if total <= 0:
        raise ResourceError(
            "MemTotal retornou valor inválido."
        )

    return MemorySnapshot(
        total_mib=total,

        available_mib=available,

        free_mib=free,

        cached_mib=cached,

        buffers_mib=buffers,

        swap_total_mib=swap_total,

        swap_free_mib=swap_free,

        swap_used_mib=swap_used,
    )


def available_memory_mib() -> int:
    """
    Atalho usado por partes antigas e futuras
    do Core.
    """

    return (
        memory_snapshot()
        .available_mib
    )


def swap_used_mib() -> int:
    """
    Retorna swap ocupada em MiB.
    """

    return (
        memory_snapshot()
        .swap_used_mib
    )


# ============================================================
# CPU
# ============================================================


def _load_average() -> tuple[
    float,
    float,
    float,
]:
    """
    Lê load average.

    Primeiro tentamos os.getloadavg(), que funciona
    em Unix/Linux.

    Mantemos /proc/loadavg como fallback.
    """

    try:
        one, five, fifteen = (
            os.getloadavg()
        )

        return (
            float(one),
            float(five),
            float(fifteen),
        )

    except (
        AttributeError,
        OSError,
    ):
        pass

    if not PROC_LOADAVG.exists():
        return (
            0.0,
            0.0,
            0.0,
        )

    try:
        content = (
            PROC_LOADAVG
            .read_text(
                encoding="utf-8"
            )
            .strip()
            .split()
        )

        if len(content) < 3:
            return (
                0.0,
                0.0,
                0.0,
            )

        return (
            float(content[0]),
            float(content[1]),
            float(content[2]),
        )

    except (
        OSError,
        ValueError,
    ):
        return (
            0.0,
            0.0,
            0.0,
        )


def cpu_snapshot() -> CpuSnapshot:
    """
    Obtém informações básicas da CPU.
    """

    logical_cpus = (
        os.cpu_count()
        or 1
    )

    architecture = (
        platform.machine()
        or "unknown"
    )

    load_1m, load_5m, load_15m = (
        _load_average()
    )

    return CpuSnapshot(
        architecture=architecture,

        logical_cpus=logical_cpus,

        load_1m=load_1m,

        load_5m=load_5m,

        load_15m=load_15m,
    )


# ============================================================
# SNAPSHOT GLOBAL
# ============================================================


def resource_snapshot() -> ResourceSnapshot:
    """
    Captura memória e CPU no mesmo momento lógico.
    """

    return ResourceSnapshot(
        memory=memory_snapshot(),

        cpu=cpu_snapshot(),
    )


# ============================================================
# PERFIS
# ============================================================


def normalize_model_name(
    model: str,
) -> str:
    """
    Normaliza nome do modelo.

    Exemplos:

        gemma3:1b
        gemma3:1b:latest

    Na prática nossos modelos normalmente já possuem tag,
    então removemos somente um ':latest' final.
    """

    model = model.strip()

    if model.endswith(
        ":latest"
    ):
        return model[:-7]

    return model


def profile_for_model(
    model: str,
    profiles: tuple[
        RuntimeProfile,
        ...,
    ] = DEFAULT_PROFILES,
) -> RuntimeProfile | None:
    """
    Localiza o perfil correspondente a um modelo.
    """

    wanted = normalize_model_name(
        model
    )

    for profile in profiles:

        current = normalize_model_name(
            profile.model
        )

        if current == wanted:
            return profile

    return None


def can_load_profile(
    profile: RuntimeProfile,
    *,
    memory: MemorySnapshot | None = None,
) -> bool:
    """
    Verifica se existe margem para CARREGAR
    determinado perfil.
    """

    if memory is None:
        memory = memory_snapshot()

    return (
        memory.available_mib
        >= profile.min_available_to_load_mib
    )


def can_keep_profile_loaded(
    profile: RuntimeProfile,
    *,
    memory: MemorySnapshot | None = None,
) -> bool:
    """
    Verifica se ainda existe margem para CONTINUAR
    usando um modelo que já está carregado.

    Essa distinção será muito importante no
    Model Manager.

    Carregar um modelo e continuar usando um modelo
    carregado são estados diferentes.
    """

    if memory is None:
        memory = memory_snapshot()

    return (
        memory.available_mib
        >= (
            profile
            .min_available_while_loaded_mib
        )
    )


def choose_profile(
    *,
    memory: MemorySnapshot | None = None,
    profiles: tuple[
        RuntimeProfile,
        ...,
    ] = DEFAULT_PROFILES,
) -> RuntimeProfile:
    """
    Seleciona o melhor perfil que cabe na RAM atual.

    A ordem de DEFAULT_PROFILES é importante:

        1. 1B
        2. 0.6B
        3. 0.5B

    Portanto sempre tentamos primeiro o perfil
    mais capaz.
    """

    if memory is None:
        memory = memory_snapshot()

    for profile in profiles:

        if can_load_profile(
            profile,
            memory=memory,
        ):
            return profile

    raise MemoryError(
        "Não há memória disponível suficiente "
        "para carregar nenhum perfil local "
        "com a margem de segurança atual. "
        f"MemAvailable={memory.available_mib} MiB."
    )


# ============================================================
# CLASSE RESOURCE MANAGER
# ============================================================


class ResourceManager:
    """
    Interface principal do módulo.

    O restante do CyberCore poderá usar esta classe
    em vez de chamar funções isoladas.
    """

    def __init__(
        self,
        profiles: tuple[
            RuntimeProfile,
            ...,
        ] = DEFAULT_PROFILES,
    ) -> None:

        if not profiles:
            raise ValueError(
                "ResourceManager precisa "
                "de pelo menos um perfil."
            )

        self.profiles = profiles

    def snapshot(
        self,
    ) -> ResourceSnapshot:
        return resource_snapshot()

    def memory(
        self,
    ) -> MemorySnapshot:
        return memory_snapshot()

    def cpu(
        self,
    ) -> CpuSnapshot:
        return cpu_snapshot()

    def choose_profile(
        self,
    ) -> RuntimeProfile:
        return choose_profile(
            profiles=self.profiles,
        )

    def profile_for_model(
        self,
        model: str,
    ) -> RuntimeProfile | None:
        return profile_for_model(
            model,
            profiles=self.profiles,
        )

    def can_load(
        self,
        profile: RuntimeProfile,
    ) -> bool:
        return can_load_profile(
            profile,
        )

    def can_keep_loaded(
        self,
        profile: RuntimeProfile,
    ) -> bool:
        return can_keep_profile_loaded(
            profile,
        )
