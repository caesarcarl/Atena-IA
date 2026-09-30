from __future__ import annotations

from cybercore.cli.main import main


def run() -> int:
    """
    Entry point do pacote CyberCore.

    Permite iniciar a aplicação com:

        python -m cybercore

    A lógica da interface permanece em:

        cybercore.cli.main

    Este arquivo existe apenas como ponto oficial
    de entrada do pacote.
    """

    return main()


if __name__ == "__main__":
    raise SystemExit(
        run()
    )
