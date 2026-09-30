# Atena 0.5.8 - Adaptive Reasoning + Cloud Presets + Chat Resilience

- reasoning por turno: auto/off/low/medium/high;
- Ollama usa reasoning adaptativo ao hardware e aceita override ATENA_OLLAMA_THINK;
- fallback non-stream quando uma resposta streaming HTTP 200 termina sem texto;
- resposta vazia deixa de ser persistida como sucesso;
- contexto local passa a respeitar a janela efetiva do runtime;
- identidade JSON compactada para gastar menos tokens;
- presets OpenAI-compatible: OpenAI, Gemini, DeepSeek, xAI/Grok e Groq;
- CLI: /provider add, /provider test, /models [provider], /reasoning;
- status expõe valores efetivos de thread/context/batch quando há overrides.

Claude/Anthropic e GitHub Copilot não são fingidos como OpenAI-compatible: entram como conectores nativos separados.
