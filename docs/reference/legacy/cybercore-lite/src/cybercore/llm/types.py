from __future__ import annotations

from dataclasses import dataclass
from typing import Literal


# ============================================================
# TIPOS BÁSICOS
# ============================================================

MessageRole = Literal[
    "system",
    "user",
    "assistant",
    "tool",
]


# ============================================================
# MENSAGENS
# ============================================================

@dataclass(slots=True, frozen=True)
class ChatMessage:
    """
    Representa uma mensagem enviada ou recebida por um LLM.

    O CyberCore usa este objeto internamente em vez de
    trabalhar diretamente com dicionários JSON.

    Exemplos de role:

        system
            Instruções gerais e identidade.

        user
            Mensagem enviada pelo usuário.

        assistant
            Mensagem produzida pelo modelo.

        tool
            Resultado de uma ferramenta futura.
    """

    role: MessageRole
    content: str

    # Campos opcionais preparados para tool calling.
    name: str | None = None
    tool_call_id: str | None = None

    def to_dict(self) -> dict:
        """
        Converte a mensagem para um dicionário compatível
        com APIs de modelos de linguagem.
        """

        data = {
            "role": self.role,
            "content": self.content,
        }

        if self.name is not None:
            data["name"] = self.name

        if self.tool_call_id is not None:
            data["tool_call_id"] = self.tool_call_id

        return data


# ============================================================
# OPÇÕES DE INFERÊNCIA
# ============================================================

@dataclass(slots=True, frozen=True)
class InferenceOptions:
    """
    Configura como uma inferência deve ser executada.

    Estes nomes são internos ao CyberCore.

    O adaptador do Ollama será responsável por converter:

        context_length
            -> num_ctx

        max_output_tokens
            -> num_predict

        num_threads
            -> num_thread

    Isso evita espalhar nomes específicos do Ollama
    pelo restante do projeto.
    """

    context_length: int = 1024

    max_output_tokens: int = 256

    num_threads: int = 2

    temperature: float = 0.2

    top_p: float = 0.9

    top_k: int = 40

    seed: int | None = None

    keep_alive: str | int = "30s"

    think: bool = False

    def __post_init__(self) -> None:
        """
        Impede a criação de configurações obviamente inválidas.
        """

        if self.context_length <= 0:
            raise ValueError(
                "context_length deve ser maior que zero"
            )

        if self.max_output_tokens <= 0:
            raise ValueError(
                "max_output_tokens deve ser maior que zero"
            )

        if self.num_threads <= 0:
            raise ValueError(
                "num_threads deve ser maior que zero"
            )

        if self.temperature < 0:
            raise ValueError(
                "temperature não pode ser negativa"
            )

        if not 0 < self.top_p <= 1:
            raise ValueError(
                "top_p deve estar entre 0 e 1"
            )

        if self.top_k < 0:
            raise ValueError(
                "top_k não pode ser negativo"
            )

    def to_dict(self) -> dict:
        """
        Converte as opções para representação genérica.

        Não é ainda o JSON do Ollama.
        """

        return {
            "context_length": self.context_length,
            "max_output_tokens": self.max_output_tokens,
            "num_threads": self.num_threads,
            "temperature": self.temperature,
            "top_p": self.top_p,
            "top_k": self.top_k,
            "seed": self.seed,
            "keep_alive": self.keep_alive,
            "think": self.think,
        }


# ============================================================
# MÉTRICAS DE INFERÊNCIA
# ============================================================

@dataclass(slots=True, frozen=True)
class InferenceMetrics:
    """
    Métricas obtidas durante uma inferência.

    O Ollama retorna tempos em nanossegundos.

    1 segundo =
        1.000.000.000 nanossegundos
    """

    total_duration_ns: int = 0

    load_duration_ns: int = 0

    prompt_eval_count: int = 0

    prompt_eval_duration_ns: int = 0

    eval_count: int = 0

    eval_duration_ns: int = 0

    # --------------------------------------------------------
    # TEMPOS
    # --------------------------------------------------------

    @property
    def total_seconds(self) -> float:
        """
        Tempo total informado pelo runtime.
        """

        return (
            self.total_duration_ns
            / 1_000_000_000
        )

    @property
    def load_seconds(self) -> float:
        """
        Tempo gasto carregando/preparando o modelo.
        """

        return (
            self.load_duration_ns
            / 1_000_000_000
        )

    @property
    def prompt_seconds(self) -> float:
        """
        Tempo gasto processando o prompt.
        """

        return (
            self.prompt_eval_duration_ns
            / 1_000_000_000
        )

    @property
    def generation_seconds(self) -> float:
        """
        Tempo gasto gerando os novos tokens.
        """

        return (
            self.eval_duration_ns
            / 1_000_000_000
        )

    @property
    def overhead_seconds(self) -> float:
        """
        Tempo que não está diretamente contabilizado como:

            carregamento
            +
            processamento do prompt
            +
            geração

        Pode incluir preparação e outros custos do runtime.
        """

        measured_ns = (
            self.load_duration_ns
            + self.prompt_eval_duration_ns
            + self.eval_duration_ns
        )

        remaining_ns = (
            self.total_duration_ns
            - measured_ns
        )

        if remaining_ns < 0:
            remaining_ns = 0

        return (
            remaining_ns
            / 1_000_000_000
        )

    # --------------------------------------------------------
    # TOKENS
    # --------------------------------------------------------

    @property
    def total_tokens(self) -> int:
        """
        Soma tokens do prompt com tokens gerados.
        """

        return (
            self.prompt_eval_count
            + self.eval_count
        )

    @property
    def prompt_tokens_per_second(self) -> float:
        """
        Velocidade de processamento do prompt.
        """

        seconds = self.prompt_seconds

        if seconds <= 0:
            return 0.0

        return (
            self.prompt_eval_count
            / seconds
        )

    @property
    def tokens_per_second(self) -> float:
        """
        Velocidade da geração da resposta.
        """

        seconds = self.generation_seconds

        if seconds <= 0:
            return 0.0

        return (
            self.eval_count
            / seconds
        )

    # --------------------------------------------------------
    # SERIALIZAÇÃO
    # --------------------------------------------------------

    def to_dict(self) -> dict:
        """
        Retorna as métricas em formato adequado para:

            logs
            benchmarks
            SQLite
            JSON
        """

        return {
            "total_duration_ns": self.total_duration_ns,
            "load_duration_ns": self.load_duration_ns,

            "prompt_eval_count": self.prompt_eval_count,
            "prompt_eval_duration_ns": (
                self.prompt_eval_duration_ns
            ),

            "eval_count": self.eval_count,
            "eval_duration_ns": (
                self.eval_duration_ns
            ),

            "total_seconds": self.total_seconds,
            "load_seconds": self.load_seconds,
            "prompt_seconds": self.prompt_seconds,
            "generation_seconds": (
                self.generation_seconds
            ),
            "overhead_seconds": (
                self.overhead_seconds
            ),

            "total_tokens": self.total_tokens,

            "prompt_tokens_per_second": (
                self.prompt_tokens_per_second
            ),

            "tokens_per_second": (
                self.tokens_per_second
            ),
        }


# ============================================================
# RESULTADO COMPLETO
# ============================================================

@dataclass(slots=True, frozen=True)
class ChatResult:
    """
    Resultado final de uma inferência sem streaming.
    """

    model: str

    content: str

    metrics: InferenceMetrics

    done_reason: str | None = None

    # Alguns modelos podem devolver raciocínio em
    # campo separado quando esse recurso estiver habilitado.
    thinking: str | None = None

    def to_dict(self) -> dict:
        """
        Representação serializável do resultado.
        """

        return {
            "model": self.model,
            "content": self.content,
            "thinking": self.thinking,
            "done_reason": self.done_reason,
            "metrics": self.metrics.to_dict(),
        }


# ============================================================
# STREAMING
# ============================================================

@dataclass(slots=True, frozen=True)
class StreamChunk:
    """
    Representa um fragmento recebido durante streaming.

    Durante a maior parte da resposta:

        done = False
        metrics = None

    No último fragmento:

        done = True

    e normalmente recebemos também as métricas completas.
    """

    content: str = ""

    model: str = ""

    done: bool = False

    done_reason: str | None = None

    thinking: str | None = None

    metrics: InferenceMetrics | None = None


# ============================================================
# MODELOS INSTALADOS
# ============================================================

@dataclass(slots=True, frozen=True)
class ModelInfo:
    """
    Informações sobre um modelo disponível localmente.

    Esta estrutura evita que o Core dependa diretamente
    do formato JSON retornado pelo Ollama.
    """

    name: str

    size_bytes: int = 0

    digest: str | None = None

    modified_at: str | None = None

    family: str | None = None

    parameter_size: str | None = None

    quantization_level: str | None = None

    @property
    def size_mib(self) -> float:
        """
        Tamanho do modelo em MiB.
        """

        return (
            self.size_bytes
            / 1024
            / 1024
        )

    @property
    def size_gib(self) -> float:
        """
        Tamanho do modelo em GiB.
        """

        return (
            self.size_bytes
            / 1024
            / 1024
            / 1024
        )

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "size_bytes": self.size_bytes,
            "size_mib": self.size_mib,
            "size_gib": self.size_gib,
            "digest": self.digest,
            "modified_at": self.modified_at,
            "family": self.family,
            "parameter_size": self.parameter_size,
            "quantization_level": (
                self.quantization_level
            ),
        }


# ============================================================
# MODELOS CARREGADOS
# ============================================================

@dataclass(slots=True, frozen=True)
class RunningModelInfo:
    """
    Informações sobre um modelo atualmente carregado
    pelo runtime.
    """

    name: str

    size_bytes: int = 0

    size_vram_bytes: int = 0

    expires_at: str | None = None

    context_length: int | None = None

    @property
    def size_mib(self) -> float:
        return (
            self.size_bytes
            / 1024
            / 1024
        )

    @property
    def size_vram_mib(self) -> float:
        return (
            self.size_vram_bytes
            / 1024
            / 1024
        )

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "size_bytes": self.size_bytes,
            "size_mib": self.size_mib,

            "size_vram_bytes": (
                self.size_vram_bytes
            ),

            "size_vram_mib": (
                self.size_vram_mib
            ),

            "expires_at": self.expires_at,

            "context_length": (
                self.context_length
            ),
        }
