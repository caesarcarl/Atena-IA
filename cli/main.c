#include "atena/client.h"
#include "atena/status.h"
#include <json-c/json.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

#define ATENA_CLI_LINE_MAX 16384
#define ATENA_CLI_PROVIDER_MAX 96

static int set_env_value(const char *name, const char *value, int overwrite) {
#ifdef _WIN32
    if (!overwrite) {
        char buf[2];
        size_t required = 0;
        if (getenv_s(&required, buf, sizeof(buf), name) == 0 && required > 0) return 0;
    }
    return _putenv_s(name, value);
#else
    return setenv(name, value, overwrite);
#endif
}

static int print_event(const AtenaStreamEvent *event, void *userdata) {
    (void)userdata;
    if (event->type == ATENA_EVENT_TEXT_DELTA && event->text) {
        fputs(event->text, stdout);
        fflush(stdout);
    } else if (event->type == ATENA_EVENT_ERROR && event->text) {
        fprintf(stderr, "\nErro: %s\n", event->text);
    }
    return 0;
}


static char *read_file_all(const char *path, size_t max_bytes) {
    if (!path || !*path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long end = ftell(f);
    if (end < 0 || (size_t)end > max_bytes) { fclose(f); errno = EFBIG; return NULL; }
    rewind(f);
    char *buf = (char *)malloc((size_t)end + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)end, f);
    fclose(f);
    if (got != (size_t)end) { free(buf); return NULL; }
    buf[got] = '\0';
    return buf;
}

static int rag_ingest_text(AtenaClient *client, const char *title, const char *locator, const char *text) {
    if (!client || !title || !locator || !text) return 1;
    json_object *params = json_object_new_object();
    if (!params) return 1;
    json_object_object_add(params, "title", json_object_new_string(title));
    json_object_object_add(params, "locator", json_object_new_string(locator));
    json_object_object_add(params, "text", json_object_new_string(text));
    const char *raw_params = json_object_to_json_string_ext(params, JSON_C_TO_STRING_PLAIN);
    char *result = NULL;
    AtenaStatus st = atena_client_call(client, "rag.ingest", raw_params, &result);
    if (st != ATENA_OK) {
        fprintf(stderr, "RAG: falha ao indexar %s: %s\n", title, atena_status_string(st));
        json_object_put(params);
        return 1;
    }
    json_object *parsed = result ? json_tokener_parse(result) : NULL;
    json_object *id = NULL;
    const char *document_id = NULL;
    if (parsed && json_object_object_get_ex(parsed, "document_id", &id)) document_id = json_object_get_string(id);
    printf("RAG: indexado: %s%s%s\n", title, document_id ? " -> " : "", document_id ? document_id : "");
    if (parsed) json_object_put(parsed);
    atena_client_free_string(result);
    json_object_put(params);
    return 0;
}

#ifndef _WIN32
static int shell_quote(const char *input, char *out, size_t cap) {
    size_t n = 0;
    if (!input || cap < 3) return 0;
    out[n++] = '\'';
    for (const char *p = input; *p; ++p) {
        if (*p == '\'') {
            const char *esc = "'\\''";
            size_t e = strlen(esc);
            if (n + e + 2 > cap) return 0;
            memcpy(out + n, esc, e); n += e;
        } else {
            if (n + 2 > cap) return 0;
            out[n++] = *p;
        }
    }
    out[n++] = '\'';
    out[n] = '\0';
    return 1;
}

static char *command_output(const char *command, int *exit_code) {
    if (exit_code) *exit_code = -1;
    FILE *pipe = popen(command, "r");
    if (!pipe) return NULL;
    size_t cap = 8192, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) { pclose(pipe); return NULL; }
    buf[0] = '\0';
    char chunk[4096];
    while (fgets(chunk, sizeof(chunk), pipe)) {
        size_t got = strlen(chunk);
        if (len + got + 1 > cap) {
            while (len + got + 1 > cap) cap *= 2;
            if (cap > 16U * 1024U * 1024U) { free(buf); pclose(pipe); errno = EFBIG; return NULL; }
            char *tmp = (char *)realloc(buf, cap);
            if (!tmp) { free(buf); pclose(pipe); return NULL; }
            buf = tmp;
        }
        memcpy(buf + len, chunk, got + 1); len += got;
    }
    int status = pclose(pipe);
    if (exit_code) {
        if (WIFEXITED(status)) *exit_code = WEXITSTATUS(status);
        else *exit_code = 128;
    }
    return buf;
}

static const char *worker_pythonpath(void) {
    const char *env = getenv("ATENA_WORKER_PYTHONPATH");
    if (env && *env) return env;
    if (access("python/atena_worker/atena_worker/__main__.py", R_OK) == 0) return "python/atena_worker";
    if (access("/usr/lib/atena/python/atena_worker/atena_worker/__main__.py", R_OK) == 0)
        return "/usr/lib/atena/python/atena_worker";
    return NULL;
}

static int rag_add_pdf_desktop(AtenaClient *client, const char *path) {
#ifndef ATENA_ENABLE_PYTHON_WORKER
    (void)client; (void)path;
    fputs("RAG: este build foi compilado sem worker Python. Indexe o PDF no desktop e copie o RAG para o dispositivo.\n", stderr);
    return 1;
#else
    const char *pythonpath = worker_pythonpath();
    if (!pythonpath) {
        fputs("RAG: worker Python não encontrado. Rode a CLI a partir da raiz do projeto ou instale o pacote desktop.\n", stderr);
        return 1;
    }
    char qpath[32768], qpython[32768], command[70000];
    if (!shell_quote(path, qpath, sizeof(qpath)) || !shell_quote(pythonpath, qpython, sizeof(qpython))) {
        fputs("RAG: caminho grande demais.\n", stderr); return 1;
    }
    int n = snprintf(command, sizeof(command), "PYTHONPATH=%s python3 -m atena_worker ingest-file %s", qpython, qpath);
    if (n < 0 || (size_t)n >= sizeof(command)) return 1;
    int rc = 0;
    char *raw = command_output(command, &rc);
    if (!raw) { perror("RAG worker"); return 1; }
    json_object *root = json_tokener_parse(raw);
    free(raw);
    if (!root || rc != 0) {
        fprintf(stderr, "RAG: worker PDF falhou (exit=%d).\n", rc);
        if (root) json_object_put(root);
        return 1;
    }
    json_object *ok = NULL, *documents = NULL, *backend = NULL;
    if (!json_object_object_get_ex(root, "ok", &ok) || !json_object_get_boolean(ok) ||
        !json_object_object_get_ex(root, "documents", &documents) || !json_object_is_type(documents, json_type_array)) {
        json_object *msg = NULL;
        if (json_object_object_get_ex(root, "message", &msg)) fprintf(stderr, "RAG: %s\n", json_object_get_string(msg));
        else fputs("RAG: resposta inválida do worker.\n", stderr);
        json_object_put(root); return 1;
    }
    if (json_object_object_get_ex(root, "backend", &backend))
        printf("RAG: PDF extraído por %s.\n", json_object_get_string(backend));
    size_t total = json_object_array_length(documents), failed = 0;
    for (size_t i = 0; i < total; ++i) {
        json_object *doc = json_object_array_get_idx(documents, i);
        json_object *title = NULL, *locator = NULL, *text = NULL;
        if (!doc || !json_object_object_get_ex(doc, "title", &title) ||
            !json_object_object_get_ex(doc, "locator", &locator) ||
            !json_object_object_get_ex(doc, "text", &text)) { failed++; continue; }
        printf("RAG: bloco %zu/%zu...\n", i + 1, total);
        failed += (size_t)rag_ingest_text(client, json_object_get_string(title),
                                          json_object_get_string(locator), json_object_get_string(text));
    }
    json_object_put(root);
    if (failed) fprintf(stderr, "RAG: %zu bloco(s) falharam.\n", failed);
    else printf("RAG: PDF concluído, %zu bloco(s) indexados.\n", total);
    return failed ? 1 : 0;
#endif
}
#endif

static int rag_add_file(AtenaClient *client, const char *path) {
    if (!path || !*path) { fputs("Uso: /rag add CAMINHO\n", stderr); return 1; }
    const char *dot = strrchr(path, '.');
    if (dot && (!strcmp(dot, ".pdf") || !strcmp(dot, ".PDF"))) {
#ifdef _WIN32
        fputs("RAG: PDF pelo worker desktop ainda está disponível primeiro no build Linux.\n", stderr);
        return 1;
#else
        return rag_add_pdf_desktop(client, path);
#endif
    }
    char *text = read_file_all(path, 3U * 1024U * 1024U);
    if (!text) {
        if (errno == EFBIG) fputs("RAG: arquivo textual > 3 MiB; use o worker/chunking.\n", stderr);
        else fprintf(stderr, "RAG: não consegui abrir %s\n", path);
        return 1;
    }
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    char locator[32768];
    int n = snprintf(locator, sizeof(locator), "file://%s", path);
    int rc = (n < 0 || (size_t)n >= sizeof(locator)) ? 1 : rag_ingest_text(client, base, locator, text);
    free(text);
    return rc;
}

static void print_help(void) {
    puts(
        "Atena 0.5 Base Integrada\n"
        "\n"
        "Uso:\n"
        "  atena                         abre a CLI persistente\n"
        "  atena run [modelo] [texto]    conversa usando Ollama\n"
        "  atena status                  estado do Core + runtime adaptativo\n"
        "  atena doctor                  diagnostico\n"
        "  atena providers               providers registrados\n"
        "  atena --demo                  inicia com provider mock\n"
        "\n"
        "Comandos dentro da CLI:\n"
        "  /help                         mostra esta ajuda\n"
        "  /new                          cria uma nova sessao\n"
        "  /status                       status completo\n"
        "  /runtime                      alias de /status; inclui RAM/CPU/plano\n"
        "  /doctor                       diagnostico do Core\n"
        "  /providers                    lista providers\n"
        "  /identity                     mostra a identidade efetiva carregada\n"
        "  /rag list                     lista documentos indexados\n"
        "  /rag add CAMINHO              adiciona TXT/MD/código ou PDF ao RAG\n"
        "  /models [PROVIDER]            lista modelos de um provider\n"
        "  /model NOME                   seleciona modelo no provider atual\n"
        "  /provider add ID MODELO       configura preset via chave do ambiente\n"
        "  /provider test ID             testa um provider configurado\n"
        "  /use PROVIDER                 usa um provider ja configurado\n"
        "  /reasoning auto|off|low|medium|high  controla raciocinio\n"
        "  !COMANDO                      shell explicito escrito pelo usuario\n"
        "  /exit | /quit                 encerra\n"
        "\n"
        "Observacao: comandos ! sao executados porque foram escritos pelo usuario.\n"
        "Tool calls produzidas por modelos continuam sujeitas ao Policy Engine."
    );
}

static int show(AtenaClient *client, const char *method, const char *params_json) {
    char *json = NULL;
    AtenaStatus status = atena_client_call(client, method, params_json ? params_json : "{}", &json);
    if (status != ATENA_OK) {
        fprintf(stderr, "Falha: %s\n", atena_status_string(status));
        return 1;
    }
    puts(json ? json : "null");
    atena_client_free_string(json);
    return 0;
}

static int ask(AtenaClient *client, const char *session, const char *provider,
               AtenaReasoningLevel reasoning, const char *text) {
    char operation[37];
    AtenaStatus status = atena_client_chat_send_ex(client, session,
                                                    provider && *provider ? provider : NULL,
                                                    text, reasoning, print_event, NULL, operation);
    putchar('\n');
    if (status != ATENA_OK) {
        fprintf(stderr, "Falha: %s\n", atena_status_string(status));
        return 1;
    }
    return 0;
}

static char *join_args(int argc, char **argv, int start) {
    size_t total = 1;
    for (int i = start; i < argc; ++i) total += strlen(argv[i]) + 1;
    char *text = (char *)calloc(total, 1);
    if (!text) return NULL;
    for (int i = start; i < argc; ++i) {
        if (i > start) strcat(text, " ");
        strcat(text, argv[i]);
    }
    return text;
}

static const char *reasoning_label(AtenaReasoningLevel level) {
    switch (level) {
        case ATENA_REASONING_DISABLED: return "off";
        case ATENA_REASONING_LOW: return "low";
        case ATENA_REASONING_MEDIUM: return "medium";
        case ATENA_REASONING_HIGH: return "high";
        default: return "auto";
    }
}

static int parse_reasoning(const char *value, AtenaReasoningLevel *out) {
    if (!value || !out) return 0;
    if (!strcmp(value,"auto")) *out=ATENA_REASONING_AUTO;
    else if (!strcmp(value,"off") || !strcmp(value,"none") || !strcmp(value,"disabled")) *out=ATENA_REASONING_DISABLED;
    else if (!strcmp(value,"low")) *out=ATENA_REASONING_LOW;
    else if (!strcmp(value,"medium")) *out=ATENA_REASONING_MEDIUM;
    else if (!strcmp(value,"high")) *out=ATENA_REASONING_HIGH;
    else return 0;
    return 1;
}

static const char *provider_key_env(const char *id) {
    if (!strcmp(id,"openai")) return "OPENAI_API_KEY";
    if (!strcmp(id,"gemini")) return "GEMINI_API_KEY";
    if (!strcmp(id,"deepseek")) return "DEEPSEEK_API_KEY";
    if (!strcmp(id,"xai")) return "XAI_API_KEY";
    if (!strcmp(id,"groq")) return "GROQ_API_KEY";
    return NULL;
}

static int provider_add_preset(AtenaClient *client, const char *id, const char *model) {
    const char *env_name = provider_key_env(id);
    if (!env_name) {
        fprintf(stderr,"Preset desconhecido: %s. Disponiveis: openai gemini deepseek xai groq\n",id);
        return 1;
    }
    const char *secret = getenv(env_name);
    if (!secret || !*secret) {
        fprintf(stderr,"Defina %s no ambiente antes de configurar %s.\n",env_name,id);
        return 1;
    }
    json_object *params=json_object_new_object();
    if(!params)return 1;
    json_object_object_add(params,"provider_id",json_object_new_string(id));
    json_object_object_add(params,"type",json_object_new_string("openai_compatible"));
    if(model&&*model)json_object_object_add(params,"model",json_object_new_string(model));
    json_object_object_add(params,"secret_value",json_object_new_string(secret));
    char *result=NULL;
    AtenaStatus st=atena_client_call(client,"providers.put",json_object_to_json_string_ext(params,JSON_C_TO_STRING_PLAIN),&result);
    json_object_put(params);
    if(st!=ATENA_OK){fprintf(stderr,"Falha ao configurar %s: %s\n",id,atena_status_string(st));return 1;}
    printf("Provider %s configurado%s%s.\n",id,model&&*model?" com modelo ":"",model&&*model?model:"");
    atena_client_free_string(result);
    return 0;
}

static int simple_json_arg(char *out, size_t out_size,
                           const char *key1, const char *value1,
                           const char *key2, const char *value2) {
    /* Provider/model IDs are deliberately restricted here. This avoids turning
       CLI input into malformed JSON without adding another parser dependency. */
    const char *values[2] = {value1, value2};
    for (size_t v = 0; v < 2; ++v) {
        if (!values[v]) continue;
        for (const char *p = values[v]; *p; ++p) {
            unsigned char c = (unsigned char)*p;
            if (c < 32 || c == '"' || c == '\\') return 0;
        }
    }
    int n;
    if (key2 && value2) {
        n = snprintf(out, out_size, "{\"%s\":\"%s\",\"%s\":\"%s\"}",
                     key1, value1, key2, value2);
    } else {
        n = snprintf(out, out_size, "{\"%s\":\"%s\"}", key1, value1);
    }
    return n >= 0 && (size_t)n < out_size;
}

static int repl(AtenaClient *client, int demo) {
    char session[37];
    AtenaStatus status = atena_client_session_create(client, "Conversa CLI", session);
    if (status != ATENA_OK) {
        fprintf(stderr, "Falha ao criar sessao: %s\n", atena_status_string(status));
        return 1;
    }

    char provider[ATENA_CLI_PROVIDER_MAX] = {0};
    AtenaReasoningLevel reasoning = ATENA_REASONING_AUTO;
    if (demo) snprintf(provider, sizeof(provider), "%s", "mock");

    puts("Atena 0.5 Base Integrada - /help mostra os comandos.");
    char line[ATENA_CLI_LINE_MAX];
    for (;;) {
        if (provider[0]) printf("atena[%s]> ", provider);
        else fputs("atena> ", stdout);
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;

        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        if (!n) continue;

        if (!strcmp(line, "/exit") || !strcmp(line, "/quit") || !strcmp(line, "/q")) break;
        if (!strcmp(line, "/help")) { print_help(); continue; }
        if (!strcmp(line, "/status") || !strcmp(line, "/runtime")) { show(client, "system.status", "{}"); continue; }
        if (!strcmp(line, "/doctor")) { show(client, "system.doctor", "{}"); continue; }
        if (!strcmp(line, "/providers")) { show(client, "providers.list", "{}"); continue; }
        if (!strcmp(line, "/identity")) { show(client, "identity.get", "{}"); continue; }
        if (!strcmp(line, "/rag list")) { show(client, "documents.list", "{}"); continue; }
        if (!strncmp(line, "/rag add ", 9)) {
            const char *path = line + 9; while (*path == ' ') ++path;
            (void)rag_add_file(client, path); continue;
        }
        if (!strcmp(line, "/rag") || !strcmp(line, "/rag help")) {
            puts("/rag add CAMINHO   indexa um arquivo local\n/rag list          lista documentos indexados");
            continue;
        }
        if (!strncmp(line, "/models", 7) && (line[7] == '\0' || line[7] == ' ')) {
            const char *id = line + 7; while (*id == ' ') ++id;
            if (!*id) id = provider[0] ? provider : "ollama";
            char params[512];
            if (!simple_json_arg(params,sizeof(params),"provider_id",id,NULL,NULL)) fputs("Provider invalido.\n",stderr);
            else show(client,"models.list",params);
            continue;
        }
        if (!strncmp(line, "/provider add ", 14)) {
            char args[1024]; snprintf(args,sizeof(args),"%s",line+14);
            char *id=strtok(args," \t"), *model=strtok(NULL,"");
            if(model)while(*model==' '||*model=='\t')model++;
            if(!id||!model||!*model)fputs("Uso: /provider add ID MODELO\n",stderr);
            else if(!provider_add_preset(client,id,model)){snprintf(provider,sizeof(provider),"%s",id);}
            continue;
        }
        if (!strncmp(line, "/provider test ", 15)) {
            const char *id=line+15; while(*id==' ')++id;
            char params[512];
            if(!*id||!simple_json_arg(params,sizeof(params),"provider_id",id,NULL,NULL))fputs("Uso: /provider test ID\n",stderr);
            else show(client,"providers.test",params);
            continue;
        }
        if (!strncmp(line, "/reasoning", 10) && (line[10]=='\0'||line[10]==' ')) {
            const char *v=line+10; while(*v==' ')++v;
            if(!*v){printf("Reasoning: %s\n",reasoning_label(reasoning));continue;}
            AtenaReasoningLevel next;
            if(!parse_reasoning(v,&next)){fputs("Uso: /reasoning auto|off|low|medium|high\n",stderr);continue;}
            reasoning=next; printf("Reasoning: %s\n",reasoning_label(reasoning)); continue;
        }
        if (!strcmp(line, "/new")) {
            if (atena_client_session_create(client, "Conversa CLI", session) == ATENA_OK) puts("Nova conversa criada.");
            else fputs("Falha ao criar nova conversa.\n", stderr);
            continue;
        }
        if (!strncmp(line, "/model ", 7)) {
            const char *model = line + 7;
            while (*model == ' ') ++model;
            const char *target_provider = provider[0] ? provider : "ollama";
            char params[1024];
            if (!*model || !simple_json_arg(params, sizeof(params), "provider_id", target_provider, "model", model)) {
                fputs("Nome de modelo invalido.\n", stderr);
                continue;
            }
            if (!show(client, "models.select", params)) {
                snprintf(provider, sizeof(provider), "%s", target_provider);
                printf("Modelo %s selecionado em %s.\n", model, target_provider);
            }
            continue;
        }
        if (!strncmp(line, "/use ", 5)) {
            const char *id = line + 5;
            while (*id == ' ') ++id;
            if (!*id || strlen(id) >= sizeof(provider)) {
                fputs("Provider invalido.\n", stderr);
                continue;
            }
            snprintf(provider, sizeof(provider), "%s", id);
            printf("Provider da sessao: %s\n", provider);
            continue;
        }
        if (line[0] == '!' && line[1]) {
            int rc = system(line + 1);
            printf("[shell exit=%d]\n", rc);
            continue;
        }

        ask(client, session, provider, reasoning, line);
    }
    return 0;
}

int main(int argc, char **argv) {
    int demo = argc > 1 && !strcmp(argv[1], "--demo");
    if (argc > 1 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        print_help();
        return 0;
    }
    if (demo) set_env_value("ATENA_ENABLE_MOCK", "1", 1);

    const char *provider = demo ? "mock" : NULL;
    int text_index = -1;
    if (argc > 1 && !strcmp(argv[1], "run")) {
        provider = "ollama";
        if (argc > 2) {
            set_env_value("ATENA_MODEL_OVERRIDE", argv[2], 1);
            set_env_value("ATENA_OLLAMA_MODEL", argv[2], 0);
        }
        if (argc > 3) text_index = 3;
    }

    AtenaClientConfig config = {0};
    config.connect_timeout_ms = 2000;
    config.request_timeout_ms = 600000;
    AtenaClient *client = NULL;
    AtenaStatus status = atena_client_connect_or_start(&config, &client);
    if (status != ATENA_OK) {
        fprintf(stderr, "Nao foi possivel iniciar a Atena: %s\n", atena_status_string(status));
        fprintf(stderr, "Verifique a instalacao com: atena doctor\n");
        return 1;
    }

    if (argc > 1 && !strcmp(argv[1], "status")) { int rc = show(client, "system.status", "{}"); atena_client_close(client); return rc; }
    if (argc > 1 && !strcmp(argv[1], "doctor")) { int rc = show(client, "system.doctor", "{}"); atena_client_close(client); return rc; }
    if (argc > 1 && !strcmp(argv[1], "providers")) { int rc = show(client, "providers.list", "{}"); atena_client_close(client); return rc; }
    if (argc > 1 && !strcmp(argv[1], "identity")) { int rc = show(client, "identity.get", "{}"); atena_client_close(client); return rc; }
    if (argc > 2 && !strcmp(argv[1], "rag") && !strcmp(argv[2], "list")) { int rc = show(client, "documents.list", "{}"); atena_client_close(client); return rc; }
    if (argc > 3 && !strcmp(argv[1], "rag") && !strcmp(argv[2], "add")) {
        char *path = join_args(argc, argv, 3);
        if (!path) { atena_client_close(client); return 1; }
        int rc = rag_add_file(client, path); free(path); atena_client_close(client); return rc;
    }

    if (text_index > 0) {
        char session[37];
        status = atena_client_session_create(client, "Conversa CLI", session);
        if (status != ATENA_OK) { atena_client_close(client); return 1; }
        char *text = join_args(argc, argv, text_index);
        if (!text) { atena_client_close(client); return 1; }
        int rc = ask(client, session, provider, ATENA_REASONING_AUTO, text);
        free(text);
        atena_client_close(client);
        return rc;
    }

    if (argc > 1 && !demo && strcmp(argv[1], "run")) {
        char session[37];
        status = atena_client_session_create(client, "Conversa CLI", session);
        if (status != ATENA_OK) { atena_client_close(client); return 1; }
        char *text = join_args(argc, argv, 1);
        if (!text) { atena_client_close(client); return 1; }
        int rc = ask(client, session, provider, ATENA_REASONING_AUTO, text);
        free(text);
        atena_client_close(client);
        return rc;
    }

    int rc = repl(client, demo);
    atena_client_close(client);
    return rc;
}
