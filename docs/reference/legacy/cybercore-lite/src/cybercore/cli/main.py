from __future__ import annotations

from cybercore.core.engine import (
    CyberCoreEngine,
    InvalidPromptError,
)
from cybercore.llm.manager import (
    ModelManagerError,
)
from cybercore.llm.ollama import (
    OllamaError,
    OllamaUnavailable,
)
from cybercore.llm.types import (
    InferenceMetrics,
)


# ============================================================
# CONSTANTES DA INTERFACE
# ============================================================

CLI_NAME = "CyberCore Lite"

PROMPT = "cyber> "


# ============================================================
# FORMATAÇÃO
# ============================================================


def format_bytes_as_mib(
    value: int,
) -> str:
    """
    Converte bytes para MiB apenas para apresentação.
    """

    mib = value / 1024 / 1024

    return f"{mib:.1f} MiB"


def format_seconds(
    value: float,
) -> str:
    """
    Formatação padronizada de tempos.
    """

    return f"{value:.2f} s"


# ============================================================
# TELEMETRIA
# ============================================================


def print_metrics(
    metrics: InferenceMetrics,
) -> None:
    """
    Exibe as métricas fornecidas pelo runtime.

    Nenhuma dessas informações é inventada pelo LLM.
    Elas vêm da execução real da inferência.
    """

    print()
    print("=== INFERÊNCIA ===")

    print(
        "Carregamento     :",
        format_seconds(
            metrics.load_seconds
        ),
    )

    print(
        "Prompt           :",
        metrics.prompt_eval_count,
        "tokens",
    )

    print(
        "Tempo do prompt  :",
        format_seconds(
            metrics.prompt_seconds
        ),
    )

    print(
        "Veloc. prompt    :",
        f"{metrics.prompt_tokens_per_second:.2f}",
        "tok/s",
    )

    print(
        "Resposta         :",
        metrics.eval_count,
        "tokens",
    )

    print(
        "Tempo geração    :",
        format_seconds(
            metrics.generation_seconds
        ),
    )

    print(
        "Veloc. geração   :",
        f"{metrics.tokens_per_second:.2f}",
        "tok/s",
    )

    print(
        "Overhead         :",
        format_seconds(
            metrics.overhead_seconds
        ),
    )

    print(
        "Tempo total      :",
        format_seconds(
            metrics.total_seconds
        ),
    )

    print("==================")


# ============================================================
# HELP
# ============================================================


def print_help() -> None:
    """
    Mostra comandos internos da CLI.

    Comandos iniciados por "/" são processados pelo Python
    e não enviados ao modelo.
    """

    print(
        """
Comandos disponíveis:

  /help
      Mostra esta ajuda.

  /status
      Mostra recursos, perfil e estado do CyberCore.

  /models
      Lista modelos instalados no Ollama.

  /ps
      Lista modelos atualmente carregados na memória.

  /plan
      Calcula qual modelo/perfil seria usado agora,
      sem executar inferência.

  /unload
      Descarrega modelos da memória.

  /reprofile
      Descarrega modelos e recalcula o melhor perfil.

  /exit
  /quit
  /q
      Encerra o CyberCore.

Qualquer outro texto será enviado ao Tartarus.
""".strip()
    )


# ============================================================
# STATUS
# ============================================================


def print_status(
    engine: CyberCoreEngine,
) -> None:
    """
    Exibe um snapshot real do CyberCore.
    """

    status = engine.status()

    memory = status.memory
    cpu = status.cpu

    print()
    print("=== CYBERCORE STATUS ===")

    print(
        "Ollama           :",
        status.ollama_version,
    )

    print()

    print("--- Memória ---")

    print(
        "RAM total        :",
        f"{memory.total_mib} MiB",
    )

    print(
        "RAM disponível   :",
        f"{memory.available_mib} MiB",
    )

    print(
        "Disponível       :",
        f"{memory.available_percent:.1f}%",
    )

    print(
        "Pressão          :",
        memory.pressure,
    )

    print(
        "Swap usada       :",
        f"{memory.swap_used_mib} MiB",
    )

    print(
        "Swap utilizada   :",
        f"{memory.swap_used_percent:.1f}%",
    )

    print()

    print("--- CPU ---")

    print(
        "Arquitetura      :",
        cpu.architecture,
    )

    print(
        "CPUs lógicas     :",
        cpu.logical_cpus,
    )

    print(
        "Load 1 min       :",
        f"{cpu.load_1m:.2f}",
    )

    print(
        "Load/CPU         :",
        f"{cpu.load_per_cpu_1m:.2f}",
    )

    print()

    print("--- Runtime ---")

    if status.running_models:

        for model in status.running_models:

            print(
                "Modelo residente :",
                model.name,
            )

            print(
                "Memória modelo    :",
                f"{model.size_mib:.1f} MiB",
            )

            if (
                model.context_length
                is not None
            ):
                print(
                    "Contexto          :",
                    model.context_length,
                )

    else:

        print(
            "Modelo residente : nenhum"
        )

    if status.active_profile:

        profile = status.active_profile

        print(
            "Perfil ativo      :",
            profile.name,
        )

        print(
            "Modelo perfil     :",
            profile.model,
        )

    else:

        print(
            "Perfil ativo      : nenhum"
        )

    print("========================")


# ============================================================
# MODELOS INSTALADOS
# ============================================================


def print_models(
    engine: CyberCoreEngine,
) -> None:
    """
    Lista modelos instalados.
    """

    models = engine.installed_models()

    print()
    print("=== MODELOS INSTALADOS ===")

    if not models:
        print("Nenhum modelo instalado.")
        return

    for model in models:

        print()
        print(
            model.name
        )

        print(
            "  tamanho       :",
            f"{model.size_mib:.1f} MiB",
        )

        if model.parameter_size:

            print(
                "  parâmetros    :",
                model.parameter_size,
            )

        if model.quantization_level:

            print(
                "  quantização   :",
                model.quantization_level,
            )

        if model.family:

            print(
                "  família       :",
                model.family,
            )

    print()


# ============================================================
# MODELOS RESIDENTES
# ============================================================


def print_running_models(
    engine: CyberCoreEngine,
) -> None:
    """
    Mostra modelos atualmente carregados pelo Ollama.
    """

    models = engine.running_models()

    print()
    print("=== MODELOS RESIDENTES ===")

    if not models:
        print("Nenhum modelo carregado.")
        print()
        return

    for model in models:

        print()
        print(model.name)

        print(
            "  tamanho       :",
            f"{model.size_mib:.1f} MiB",
        )

        print(
            "  VRAM          :",
            f"{model.size_vram_mib:.1f} MiB",
        )

        if model.context_length is not None:

            print(
                "  contexto      :",
                model.context_length,
            )

        if model.expires_at:

            print(
                "  expira em     :",
                model.expires_at,
            )

    print()


# ============================================================
# PLANO
# ============================================================


def print_plan(
    engine: CyberCoreEngine,
) -> None:
    """
    Solicita ao ModelManager o plano que seria usado
    para uma nova inferência.

    prepare() não carrega o modelo novo.
    """

    plan = engine.prepare()

    print()
    print("=== PLANO DE EXECUÇÃO ===")

    print(
        "Perfil           :",
        plan.profile.name,
    )

    print(
        "Modelo           :",
        plan.model,
    )

    print(
        "Origem           :",
        plan.origin,
    )

    print(
        "RAM disponível   :",
        f"{plan.available_memory_mib} MiB",
    )

    print(
        "Contexto         :",
        plan.options.context_length,
    )

    print(
        "Saída máxima     :",
        plan.options.max_output_tokens,
        "tokens",
    )

    print(
        "Threads          :",
        plan.options.num_threads,
    )

    print(
        "Temperature      :",
        plan.options.temperature,
    )

    print(
        "Keep alive       :",
        plan.options.keep_alive,
    )

    print(
        "Thinking         :",
        plan.options.think,
    )

    print()

    if plan.running_before:

        print(
            "Antes havia      :",
            ", ".join(
                plan.running_before
            ),
        )

    else:

        print(
            "Antes havia      : nenhum modelo"
        )

    print("=========================")


# ============================================================
# UNLOAD
# ============================================================


def unload_models(
    engine: CyberCoreEngine,
) -> None:
    """
    Libera modelos residentes.
    """

    engine.unload()

    print(
        "Modelos descarregados."
    )


# ============================================================
# REPROFILE
# ============================================================


def reprofile(
    engine: CyberCoreEngine,
) -> None:
    """
    Força nova escolha de perfil.
    """

    plan = engine.reprofile()

    print()
    print("Novo perfil selecionado:")

    print(
        "  perfil :",
        plan.profile.name,
    )

    print(
        "  modelo :",
        plan.model,
    )

    print(
        "  RAM    :",
        f"{plan.available_memory_mib} MiB",
    )

    print(
        "  ctx    :",
        plan.options.context_length,
    )

    print()


# ============================================================
# INFERÊNCIA
# ============================================================


def run_inference(
    engine: CyberCoreEngine,
    prompt: str,
) -> None:
    """
    Executa uma inferência em streaming.

    A CLI apenas imprime os chunks recebidos.

    Toda decisão sobre modelo e recursos permanece no Core.
    """

    final_metrics = None

    print()

    for chunk in engine.stream(
        prompt
    ):

        if chunk.content:

            print(
                chunk.content,
                end="",
                flush=True,
            )

        if chunk.done:

            final_metrics = (
                chunk.metrics
            )

    print()

    if final_metrics is not None:

        print_metrics(
            final_metrics
        )


# ============================================================
# PROCESSAMENTO DE COMANDOS
# ============================================================


def handle_command(
    engine: CyberCoreEngine,
    text: str,
) -> bool:
    """
    Processa comandos internos.

    Retorno:

        True
            comando foi processado

        False
            não era comando; deve ir para o LLM
    """

    command = text.strip().lower()

    if command == "/help":

        print_help()
        return True

    if command == "/status":

        print_status(
            engine
        )
        return True

    if command == "/models":

        print_models(
            engine
        )
        return True

    if command == "/ps":

        print_running_models(
            engine
        )
        return True

    if command == "/plan":

        print_plan(
            engine
        )
        return True

    if command == "/unload":

        unload_models(
            engine
        )
        return True

    if command == "/reprofile":

        reprofile(
            engine
        )
        return True

    return False


# ============================================================
# BANNER
# ============================================================


def print_banner() -> None:
    """
    Cabeçalho inicial da aplicação.
    """

    print()
    print(CLI_NAME)
    print("=" * len(CLI_NAME))
    print(
        "Digite /help para ajuda."
    )
    print()


# ============================================================
# MAIN
# ============================================================


def main() -> int:
    """
    Entry point principal da CLI.

    Retorna código de saída Unix:

        0 = encerramento normal
        1 = erro de inicialização
    """

    engine = CyberCoreEngine()

    print_banner()

    # --------------------------------------------------------
    # VERIFICAÇÃO INICIAL
    # --------------------------------------------------------

    if not engine.is_ollama_available():

        print(
            "Erro: servidor Ollama "
            "não está acessível."
        )

        print(
            "Verifique o serviço com:"
        )

        print(
            "  systemctl status ollama"
        )

        return 1

    # --------------------------------------------------------
    # LOOP PRINCIPAL
    # --------------------------------------------------------

    while True:

        try:

            text = input(
                PROMPT
            ).strip()

        except EOFError:

            print()
            break

        except KeyboardInterrupt:

            print()
            print(
                "Use /exit para encerrar."
            )

            continue

        # ----------------------------------------------------
        # ENTRADA VAZIA
        # ----------------------------------------------------

        if not text:
            continue

        # ----------------------------------------------------
        # SAÍDA
        # ----------------------------------------------------

        if text.lower() in {
            "/exit",
            "/quit",
            "/q",
        }:

            break

        # ----------------------------------------------------
        # COMANDOS INTERNOS
        # ----------------------------------------------------

        try:

            if handle_command(
                engine,
                text,
            ):
                continue

        except MemoryError as error:

            print(
                "Resource Manager:",
                error,
            )

            continue

        except ModelManagerError as error:

            print(
                "Model Manager:",
                error,
            )

            continue

        except OllamaError as error:

            print(
                "Erro Ollama:",
                error,
            )

            continue

        # ----------------------------------------------------
        # INFERÊNCIA
        # ----------------------------------------------------

        try:

            run_inference(
                engine,
                text,
            )

        except InvalidPromptError as error:

            print(
                "Prompt inválido:",
                error,
            )

        except MemoryError as error:

            print()
            print(
                "Resource Manager:",
                error,
            )

        except ModelManagerError as error:

            print()
            print(
                "Model Manager:",
                error,
            )

        except OllamaUnavailable as error:

            print()
            print(
                "Ollama indisponível:",
                error,
            )

        except OllamaError as error:

            print()
            print(
                "Erro Ollama:",
                error,
            )

        except KeyboardInterrupt:

            print()
            print()
            print(
                "Inferência interrompida "
                "pelo usuário."
            )

    # --------------------------------------------------------
    # ENCERRAMENTO
    # --------------------------------------------------------

    print()
    print(
        "CyberCore encerrado."
    )

    return 0


# ============================================================
# EXECUÇÃO DIRETA
# ============================================================


if __name__ == "__main__":

    raise SystemExit(
        main()
    )
