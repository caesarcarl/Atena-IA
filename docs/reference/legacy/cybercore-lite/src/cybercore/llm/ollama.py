from __future__ import annotations

import json
from collections.abc import Iterable, Iterator
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

from cybercore.llm.types import (
    ChatMessage,
    ChatResult,
    InferenceMetrics,
    InferenceOptions,
    ModelInfo,
    RunningModelInfo,
    StreamChunk,
)


# ============================================================
# EXCEÇÕES
# ============================================================


class OllamaError(RuntimeError):
    """
    Erro genérico relacionado ao runtime Ollama.
    """


class OllamaUnavailable(OllamaError):
    """
    O servidor Ollama não pôde ser alcançado.
    """


class OllamaHTTPError(OllamaError):
    """
    O Ollama respondeu com um erro HTTP.
    """

    def __init__(
        self,
        status_code: int,
        message: str,
    ) -> None:
        self.status_code = status_code
        self.message = message

        super().__init__(
            f"Ollama respondeu HTTP "
            f"{status_code}: {message}"
        )


class OllamaProtocolError(OllamaError):
    """
    A resposta recebida do Ollama não estava
    no formato esperado.
    """


# ============================================================
# CLIENTE
# ============================================================


class OllamaClient:
    """
    Cliente HTTP leve para o servidor Ollama.

    Não depende do pacote Python oficial do Ollama
    nem de bibliotecas externas como requests/httpx.

    CyberCore
        ↓
    OllamaClient
        ↓
    HTTP
        ↓
    Ollama
        ↓
    Modelo local
    """

    def __init__(
        self,
        base_url: str = "http://127.0.0.1:11434",
        timeout: float = 600.0,
    ) -> None:
        """
        Parameters
        ----------
        base_url:
            Endereço do servidor Ollama.

        timeout:
            Timeout máximo em segundos para
            operações HTTP.

            O valor é propositalmente alto porque
            estamos trabalhando com hardware lento.
        """

        self.base_url = base_url.rstrip("/")
        self.timeout = timeout

    # ========================================================
    # UTILIDADES INTERNAS
    # ========================================================

    def _url(
        self,
        path: str,
    ) -> str:
        """
        Monta uma URL completa.
        """

        if not path.startswith("/"):
            path = "/" + path

        return f"{self.base_url}{path}"

    @staticmethod
    def _normalize_model_name(
        name: str,
    ) -> str:
        """
        Normaliza nomes para comparação.

        Exemplo:

            modelo
            modelo:latest

        são considerados equivalentes.
        """

        name = name.strip()

        if name.endswith(":latest"):
            return name[:-7]

        return name

    # ========================================================
    # HTTP
    # ========================================================

    def _open(
        self,
        request: Request,
        *,
        timeout: float | None = None,
    ):
        """
        Executa uma requisição HTTP.

        Centralizamos tratamento de erros aqui para que
        o restante do cliente não precise repetir lógica.
        """

        try:
            return urlopen(
                request,
                timeout=(
                    self.timeout
                    if timeout is None
                    else timeout
                ),
            )

        except HTTPError as exc:
            try:
                raw = exc.read()
                message = raw.decode(
                    "utf-8",
                    errors="replace",
                )

                try:
                    data = json.loads(message)

                    if isinstance(data, dict):
                        message = str(
                            data.get(
                                "error",
                                message,
                            )
                        )

                except json.JSONDecodeError:
                    pass

            except Exception:
                message = str(exc.reason)

            raise OllamaHTTPError(
                status_code=exc.code,
                message=message,
            ) from exc

        except URLError as exc:
            raise OllamaUnavailable(
                "Não foi possível conectar ao Ollama "
                f"em {self.base_url}: {exc.reason}"
            ) from exc

        except TimeoutError as exc:
            raise OllamaUnavailable(
                "Tempo limite excedido ao comunicar "
                "com o Ollama."
            ) from exc

    def _request_json(
        self,
        method: str,
        path: str,
        payload: dict[str, Any] | None = None,
        *,
        timeout: float | None = None,
    ) -> dict[str, Any]:
        """
        Executa requisição e espera um único objeto JSON.
        """

        headers = {
            "Accept": "application/json",
        }

        body = None

        if payload is not None:
            body = json.dumps(
                payload,
                ensure_ascii=False,
            ).encode("utf-8")

            headers[
                "Content-Type"
            ] = "application/json"

        request = Request(
            url=self._url(path),
            data=body,
            headers=headers,
            method=method.upper(),
        )

        with self._open(
            request,
            timeout=timeout,
        ) as response:
            raw = response.read()

        if not raw:
            return {}

        try:
            data = json.loads(
                raw.decode("utf-8")
            )

        except json.JSONDecodeError as exc:
            raise OllamaProtocolError(
                "O Ollama retornou JSON inválido."
            ) from exc

        if not isinstance(data, dict):
            raise OllamaProtocolError(
                "Era esperado um objeto JSON "
                "como resposta do Ollama."
            )

        error = data.get("error")

        if error:
            raise OllamaError(
                str(error)
            )

        return data

    # ========================================================
    # ESTADO DO SERVIDOR
    # ========================================================

    def version(self) -> str:
        """
        Retorna a versão do Ollama.
        """

        data = self._request_json(
            "GET",
            "/api/version",
        )

        return str(
            data.get(
                "version",
                "unknown",
            )
        )

    def ping(self) -> bool:
        """
        Verifica se o servidor está acessível.
        """

        try:
            self.version()
            return True

        except OllamaError:
            return False

    # ========================================================
    # MODELOS INSTALADOS
    # ========================================================

    def list_models(
        self,
    ) -> list[ModelInfo]:
        """
        Retorna os modelos instalados localmente.
        """

        data = self._request_json(
            "GET",
            "/api/tags",
        )

        raw_models = data.get(
            "models",
            [],
        )

        if not isinstance(
            raw_models,
            list,
        ):
            raise OllamaProtocolError(
                "Campo 'models' inválido "
                "em /api/tags."
            )

        result: list[ModelInfo] = []

        for item in raw_models:
            if not isinstance(
                item,
                dict,
            ):
                continue

            name = str(
                item.get("name")
                or item.get("model")
                or ""
            )

            if not name:
                continue

            details = item.get(
                "details"
            )

            if not isinstance(
                details,
                dict,
            ):
                details = {}

            result.append(
                ModelInfo(
                    name=name,

                    size_bytes=int(
                        item.get(
                            "size",
                            0,
                        )
                        or 0
                    ),

                    digest=(
                        str(item["digest"])
                        if item.get(
                            "digest"
                        )
                        else None
                    ),

                    modified_at=(
                        str(
                            item[
                                "modified_at"
                            ]
                        )
                        if item.get(
                            "modified_at"
                        )
                        else None
                    ),

                    family=(
                        str(
                            details[
                                "family"
                            ]
                        )
                        if details.get(
                            "family"
                        )
                        else None
                    ),

                    parameter_size=(
                        str(
                            details[
                                "parameter_size"
                            ]
                        )
                        if details.get(
                            "parameter_size"
                        )
                        else None
                    ),

                    quantization_level=(
                        str(
                            details[
                                "quantization_level"
                            ]
                        )
                        if details.get(
                            "quantization_level"
                        )
                        else None
                    ),
                )
            )

        return result

    def has_model(
        self,
        model: str,
    ) -> bool:
        """
        Verifica se um modelo existe localmente.
        """

        wanted = (
            self._normalize_model_name(
                model
            )
        )

        for installed in (
            self.list_models()
        ):
            current = (
                self._normalize_model_name(
                    installed.name
                )
            )

            if current == wanted:
                return True

        return False

    def find_model(
        self,
        model: str,
    ) -> ModelInfo | None:
        """
        Procura um modelo instalado e retorna
        seus metadados.
        """

        wanted = (
            self._normalize_model_name(
                model
            )
        )

        for installed in (
            self.list_models()
        ):
            current = (
                self._normalize_model_name(
                    installed.name
                )
            )

            if current == wanted:
                return installed

        return None

    # ========================================================
    # MODELOS CARREGADOS
    # ========================================================

    def running_models(
        self,
    ) -> list[RunningModelInfo]:
        """
        Retorna modelos atualmente carregados
        pelo Ollama.
        """

        data = self._request_json(
            "GET",
            "/api/ps",
        )

        raw_models = data.get(
            "models",
            [],
        )

        if not isinstance(
            raw_models,
            list,
        ):
            raise OllamaProtocolError(
                "Campo 'models' inválido "
                "em /api/ps."
            )

        result: list[
            RunningModelInfo
        ] = []

        for item in raw_models:
            if not isinstance(
                item,
                dict,
            ):
                continue

            name = str(
                item.get("name")
                or item.get("model")
                or ""
            )

            if not name:
                continue

            context_length = (
                item.get(
                    "context_length"
                )
            )

            if context_length is not None:
                try:
                    context_length = int(
                        context_length
                    )

                except (
                    TypeError,
                    ValueError,
                ):
                    context_length = None

            result.append(
                RunningModelInfo(
                    name=name,

                    size_bytes=int(
                        item.get(
                            "size",
                            0,
                        )
                        or 0
                    ),

                    size_vram_bytes=int(
                        item.get(
                            "size_vram",
                            0,
                        )
                        or 0
                    ),

                    expires_at=(
                        str(
                            item[
                                "expires_at"
                            ]
                        )
                        if item.get(
                            "expires_at"
                        )
                        else None
                    ),

                    context_length=(
                        context_length
                    ),
                )
            )

        return result

    def is_model_running(
        self,
        model: str,
    ) -> bool:
        """
        Verifica se determinado modelo está
        atualmente carregado.
        """

        wanted = (
            self._normalize_model_name(
                model
            )
        )

        for running in (
            self.running_models()
        ):
            current = (
                self._normalize_model_name(
                    running.name
                )
            )

            if current == wanted:
                return True

        return False

    # ========================================================
    # CONVERSÃO DE OPÇÕES
    # ========================================================

    @staticmethod
    def _ollama_options(
        options: InferenceOptions,
    ) -> dict[str, Any]:
        """
        Traduz as opções genéricas do CyberCore
        para os nomes usados pelo Ollama.

        CyberCore:
            context_length

        Ollama:
            num_ctx
        """

        result: dict[str, Any] = {
            "num_ctx": (
                options.context_length
            ),

            "num_predict": (
                options.max_output_tokens
            ),

            "num_thread": (
                options.num_threads
            ),

            "temperature": (
                options.temperature
            ),

            "top_p": (
                options.top_p
            ),

            "top_k": (
                options.top_k
            ),
        }

        if options.seed is not None:
            result["seed"] = (
                options.seed
            )

        return result

    # ========================================================
    # MÉTRICAS
    # ========================================================

    @staticmethod
    def _metrics_from_response(
        data: dict[str, Any],
    ) -> InferenceMetrics:
        """
        Converte métricas do Ollama para
        InferenceMetrics.
        """

        return InferenceMetrics(
            total_duration_ns=int(
                data.get(
                    "total_duration",
                    0,
                )
                or 0
            ),

            load_duration_ns=int(
                data.get(
                    "load_duration",
                    0,
                )
                or 0
            ),

            prompt_eval_count=int(
                data.get(
                    "prompt_eval_count",
                    0,
                )
                or 0
            ),

            prompt_eval_duration_ns=int(
                data.get(
                    "prompt_eval_duration",
                    0,
                )
                or 0
            ),

            eval_count=int(
                data.get(
                    "eval_count",
                    0,
                )
                or 0
            ),

            eval_duration_ns=int(
                data.get(
                    "eval_duration",
                    0,
                )
                or 0
            ),
        )

    # ========================================================
    # PAYLOAD DE CHAT
    # ========================================================

    def _chat_payload(
        self,
        model: str,
        messages: Iterable[
            ChatMessage
        ],
        options: InferenceOptions,
        *,
        stream: bool,
    ) -> dict[str, Any]:
        """
        Constrói o payload usado em /api/chat.
        """

        payload: dict[str, Any] = {
            "model": model,

            "messages": [
                message.to_dict()
                for message in messages
            ],

            "stream": stream,

            "keep_alive": (
                options.keep_alive
            ),

            "think": (
                options.think
            ),

            "options": (
                self._ollama_options(
                    options
                )
            ),
        }

        return payload

    # ========================================================
    # CHAT SEM STREAMING
    # ========================================================

    def chat(
        self,
        model: str,
        messages: Iterable[
            ChatMessage
        ],
        *,
        options: (
            InferenceOptions
            | None
        ) = None,
    ) -> ChatResult:
        """
        Executa uma inferência completa sem streaming.
        """

        if options is None:
            options = (
                InferenceOptions()
            )

        payload = self._chat_payload(
            model=model,
            messages=messages,
            options=options,
            stream=False,
        )

        data = self._request_json(
            "POST",
            "/api/chat",
            payload,
        )

        raw_message = data.get(
            "message",
            {},
        )

        if not isinstance(
            raw_message,
            dict,
        ):
            raise OllamaProtocolError(
                "Campo 'message' inválido "
                "na resposta de /api/chat."
            )

        content = str(
            raw_message.get(
                "content",
                "",
            )
            or ""
        )

        thinking = raw_message.get(
            "thinking"
        )

        if thinking is not None:
            thinking = str(
                thinking
            )

        return ChatResult(
            model=str(
                data.get(
                    "model",
                    model,
                )
            ),

            content=content,

            thinking=thinking,

            done_reason=(
                str(
                    data[
                        "done_reason"
                    ]
                )
                if data.get(
                    "done_reason"
                )
                else None
            ),

            metrics=(
                self._metrics_from_response(
                    data
                )
            ),
        )

    # ========================================================
    # CHAT COM STREAMING
    # ========================================================

    def stream_chat(
        self,
        model: str,
        messages: Iterable[
            ChatMessage
        ],
        *,
        options: (
            InferenceOptions
            | None
        ) = None,
    ) -> Iterator[
        StreamChunk
    ]:
        """
        Executa inferência utilizando streaming.

        O Ollama retorna vários objetos JSON,
        normalmente um por linha.
        """

        if options is None:
            options = (
                InferenceOptions()
            )

        payload = self._chat_payload(
            model=model,
            messages=messages,
            options=options,
            stream=True,
        )

        body = json.dumps(
            payload,
            ensure_ascii=False,
        ).encode("utf-8")

        request = Request(
            url=self._url(
                "/api/chat"
            ),

            data=body,

            headers={
                "Content-Type":
                    "application/json",

                "Accept":
                    "application/x-ndjson",
            },

            method="POST",
        )

        with self._open(
            request
        ) as response:

            for raw_line in response:

                if not raw_line:
                    continue

                line = raw_line.decode(
                    "utf-8",
                    errors="replace",
                ).strip()

                if not line:
                    continue

                try:
                    data = json.loads(
                        line
                    )

                except json.JSONDecodeError as exc:
                    raise OllamaProtocolError(
                        "O Ollama retornou "
                        "um chunk JSON inválido."
                    ) from exc

                if not isinstance(
                    data,
                    dict,
                ):
                    raise OllamaProtocolError(
                        "Chunk de streaming "
                        "não é um objeto JSON."
                    )

                error = data.get(
                    "error"
                )

                if error:
                    raise OllamaError(
                        str(error)
                    )

                raw_message = data.get(
                    "message",
                    {},
                )

                if not isinstance(
                    raw_message,
                    dict,
                ):
                    raw_message = {}

                done = bool(
                    data.get(
                        "done",
                        False,
                    )
                )

                thinking = (
                    raw_message.get(
                        "thinking"
                    )
                )

                if thinking is not None:
                    thinking = str(
                        thinking
                    )

                metrics = None

                if done:
                    metrics = (
                        self._metrics_from_response(
                            data
                        )
                    )

                yield StreamChunk(
                    model=str(
                        data.get(
                            "model",
                            model,
                        )
                    ),

                    content=str(
                        raw_message.get(
                            "content",
                            "",
                        )
                        or ""
                    ),

                    thinking=thinking,

                    done=done,

                    done_reason=(
                        str(
                            data[
                                "done_reason"
                            ]
                        )
                        if data.get(
                            "done_reason"
                        )
                        else None
                    ),

                    metrics=metrics,
                )

    # ========================================================
    # CICLO DE VIDA DO MODELO
    # ========================================================

    def load_model(
        self,
        model: str,
        *,
        keep_alive: str | int = "30s",
    ) -> None:
        """
        Solicita que o Ollama carregue o modelo.

        Não gera resposta ao usuário.
        """

        payload = {
            "model": model,
            "messages": [],
            "stream": False,
            "keep_alive": keep_alive,
        }

        self._request_json(
            "POST",
            "/api/chat",
            payload,
        )

    def unload_model(
        self,
        model: str,
    ) -> None:
        """
        Solicita que o modelo seja removido
        da memória.
        """

        payload = {
            "model": model,
            "messages": [],
            "stream": False,
            "keep_alive": 0,
        }

        self._request_json(
            "POST",
            "/api/chat",
            payload,
        )

    def unload_all(
        self,
    ) -> None:
        """
        Descarrega todos os modelos que
        o Ollama informa como residentes.
        """

        running = (
            self.running_models()
        )

        for model in running:
            self.unload_model(
                model.name
            )

    def unload_all_except(
        self,
        wanted_model: str,
    ) -> None:
        """
        Mantém somente o modelo solicitado.

        Isso será importante no notebook de
        pouca RAM para impedir que múltiplos
        modelos permaneçam residentes.
        """

        wanted = (
            self._normalize_model_name(
                wanted_model
            )
        )

        for running in (
            self.running_models()
        ):
            current = (
                self._normalize_model_name(
                    running.name
                )
            )

            if current != wanted:
                self.unload_model(
                    running.name
                )
