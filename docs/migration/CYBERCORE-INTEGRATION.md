# Integração do CyberCore

O CyberCore original foi mantido em `reference/cybercore-lite/` apenas como referência de migração.
Ele não é executado pelo Atena e não cria um segundo daemon.

Já migrado:

- snapshot de RAM/CPU;
- classificação de pressão de memória;
- perfil de execução;
- recomendação de threads/contexto;
- residência de modelo como política (`keep_alive`);
- aplicação das recomendações ao request Ollama.

Ainda a migrar:

- benchmark persistente por modelo/dispositivo;
- model registry;
- warm/unload explícito;
- detecção de modelos residentes (`/api/ps`);
- reprofile/autotune;
- métricas completas de load/prefill/decode.
