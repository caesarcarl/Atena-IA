#include "atena/provider.h"
#include "atena/sync.h"

#include <curl/curl.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OPENAI_COMPAT_DEFAULT_TIMEOUT_MS 600000L

typedef struct OpenAiCompatImpl {
    char base_url[768];
    char bearer_token[2048];
    AtenaMutex lock;
    char active_operation[37];
    int cancelled;
} OpenAiCompatImpl;

typedef struct HttpBuffer {
    char *data;
    size_t size;
} HttpBuffer;

typedef struct SseContext {
    OpenAiCompatImpl *impl;
    const char *operation_id;
    AtenaEventCallback callback;
    void *userdata;
    char *pending;
    size_t pending_size;
    uint64_t sequence;
    size_t text_bytes;
    size_t reasoning_bytes;
    AtenaStatus status;
} SseContext;

static AtenaOnce curl_once = ATENA_ONCE_INIT;
static void initialize_curl(void) { (void)curl_global_init(CURL_GLOBAL_DEFAULT); }

static const char *role_name(AtenaRole role) {
    switch (role) {
        case ATENA_ROLE_SYSTEM: return "system";
        case ATENA_ROLE_USER: return "user";
        case ATENA_ROLE_ASSISTANT: return "assistant";
        case ATENA_ROLE_TOOL: return "tool";
    }
    return "user";
}

static int is_cancelled(OpenAiCompatImpl *impl) {
    int value;
    atena_mutex_lock(&impl->lock);
    value = impl->cancelled;
    atena_mutex_unlock(&impl->lock);
    return value;
}

static struct curl_slist *make_headers(const OpenAiCompatImpl *impl) {
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: text/event-stream, application/json");
    if (impl && impl->bearer_token[0]) {
        char authorization[2300];
        int n = snprintf(authorization, sizeof(authorization), "Authorization: Bearer %s", impl->bearer_token);
        if (n > 0 && (size_t)n < sizeof(authorization)) headers = curl_slist_append(headers, authorization);
    }
    return headers;
}

static int build_url(const OpenAiCompatImpl *impl, const char *path, char *out, size_t cap) {
    if (!impl || !path || !out || cap == 0) return 0;
    const size_t len = strlen(impl->base_url);
    const int base_slash = len > 0 && impl->base_url[len - 1] == '/';
    const int path_slash = path[0] == '/';
    int n = snprintf(out, cap, base_slash && path_slash ? "%s%s" : (!base_slash && !path_slash ? "%s/%s" : "%s%s"),
                     impl->base_url, (base_slash && path_slash) ? path + 1 : path);
    return n >= 0 && (size_t)n < cap;
}

static size_t collect_http(char *data, size_t size, size_t count, void *userdata) {
    HttpBuffer *buffer = userdata;
    size_t bytes = size * count;
    if (!bytes) return 0;
    char *next = realloc(buffer->data, buffer->size + bytes + 1);
    if (!next) return 0;
    buffer->data = next;
    memcpy(buffer->data + buffer->size, data, bytes);
    buffer->size += bytes;
    buffer->data[buffer->size] = '\0';
    return bytes;
}

static AtenaStatus http_get(OpenAiCompatImpl *impl, const char *path, long timeout_ms, char **out_body) {
    if (!impl || !path || !out_body) return ATENA_ERR_INVALID_ARGUMENT;
    *out_body = NULL;
    atena_once(&curl_once, initialize_curl);

    char url[1024];
    if (!build_url(impl, path, url, sizeof(url))) return ATENA_ERR_INVALID_ARGUMENT;
    CURL *curl = curl_easy_init();
    if (!curl) return ATENA_ERR_INTERNAL;
    HttpBuffer buffer = {0};
    struct curl_slist *headers = make_headers(impl);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect_http);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 8000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    CURLcode result = curl_easy_perform(curl);
    long http = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (result == CURLE_OPERATION_TIMEDOUT) { free(buffer.data); return ATENA_ERR_TIMEOUT; }
    if (result != CURLE_OK) { free(buffer.data); return ATENA_ERR_NETWORK; }
    if (http == 401 || http == 403) { free(buffer.data); return ATENA_ERR_PERMISSION; }
    if (http < 200 || http >= 300) { free(buffer.data); return ATENA_ERR_PROVIDER_INVALID; }
    if (!buffer.data) {
        buffer.data = calloc(1, 1);
        if (!buffer.data) return ATENA_ERR_NO_MEMORY;
    }
    *out_body = buffer.data;
    return ATENA_OK;
}

static int emit_sse_payload(SseContext *ctx, const char *payload) {
    if (!payload || !*payload) return 1;
    while (*payload == ' ' || *payload == '\t') payload++;
    if (!strcmp(payload, "[DONE]")) return 1;

    json_object *root = json_tokener_parse(payload);
    if (!root) { ctx->status = ATENA_ERR_JSON; return 0; }
    json_object *error = NULL;
    if (json_object_object_get_ex(root, "error", &error)) {
        ctx->status = ATENA_ERR_PROVIDER_INVALID;
        json_object_put(root);
        return 0;
    }

    json_object *choices = NULL;
    if (json_object_object_get_ex(root, "choices", &choices) && json_object_is_type(choices, json_type_array) && json_object_array_length(choices) > 0) {
        json_object *choice = json_object_array_get_idx(choices, 0);
        json_object *delta = NULL, *content = NULL;
        if (choice && json_object_object_get_ex(choice, "delta", &delta) &&
            json_object_is_type(delta, json_type_object) &&
            json_object_object_get_ex(delta, "content", &content) &&
            json_object_is_type(content, json_type_string)) {
            const char *text = json_object_get_string(content);
            if (text && *text) {
                ctx->text_bytes += strlen(text);
                AtenaStreamEvent event = {0};
                event.type = ATENA_EVENT_TEXT_DELTA;
                event.operation_id = ctx->operation_id;
                event.seq = ++ctx->sequence;
                event.text = text;
                if (ctx->callback(&event, ctx->userdata) != 0) {
                    ctx->status = ATENA_ERR_CANCELLED;
                    json_object_put(root);
                    return 0;
                }
            }
        }
    }

    if (json_object_object_get_ex(root, "choices", &choices) && json_object_is_type(choices, json_type_array) && json_object_array_length(choices) > 0) {
        json_object *choice = json_object_array_get_idx(choices, 0);
        json_object *delta = NULL, *reason = NULL;
        if (choice && json_object_object_get_ex(choice, "delta", &delta) && json_object_is_type(delta, json_type_object)) {
            if (json_object_object_get_ex(delta, "reasoning_content", &reason) && json_object_is_type(reason, json_type_string)) {
                const char *text = json_object_get_string(reason);
                if (text) ctx->reasoning_bytes += strlen(text);
                const char *expose = getenv("ATENA_EXPOSE_MODEL_THINKING");
                if (text && *text && expose && !strcmp(expose,"1")) {
                    AtenaStreamEvent event = {0}; event.type=ATENA_EVENT_REASONING_DELTA;
                    event.operation_id=ctx->operation_id; event.seq=++ctx->sequence; event.text=text;
                    if (ctx->callback(&event,ctx->userdata)!=0) { ctx->status=ATENA_ERR_CANCELLED; json_object_put(root); return 0; }
                }
            }
        }
    }

    json_object *usage = NULL;
    if (json_object_object_get_ex(root, "usage", &usage) && json_object_is_type(usage, json_type_object)) {
        AtenaStreamEvent event = {0};
        event.type = ATENA_EVENT_USAGE;
        event.operation_id = ctx->operation_id;
        event.seq = ++ctx->sequence;
        json_object *v = NULL;
        if (json_object_object_get_ex(usage, "prompt_tokens", &v)) event.metrics.prompt_tokens = (uint64_t)json_object_get_int64(v);
        if (json_object_object_get_ex(usage, "completion_tokens", &v)) event.metrics.completion_tokens = (uint64_t)json_object_get_int64(v);
        (void)ctx->callback(&event, ctx->userdata);
    }
    json_object_put(root);
    return 1;
}

static int process_line(SseContext *ctx, const char *line, size_t length) {
    if (!length) return 1;
    while (length && (line[length - 1] == '\r' || line[length - 1] == '\n')) length--;
    if (!length) return 1;
    if (length < 5 || strncmp(line, "data:", 5) != 0) return 1;
    char *copy = malloc(length - 5 + 1);
    if (!copy) { ctx->status = ATENA_ERR_NO_MEMORY; return 0; }
    memcpy(copy, line + 5, length - 5);
    copy[length - 5] = '\0';
    int ok = emit_sse_payload(ctx, copy);
    free(copy);
    return ok;
}

static size_t receive_sse(char *data, size_t size, size_t count, void *userdata) {
    SseContext *ctx = userdata;
    size_t bytes = size * count;
    if (!bytes) return 0;
    if (is_cancelled(ctx->impl)) { ctx->status = ATENA_ERR_CANCELLED; return 0; }
    char *next = realloc(ctx->pending, ctx->pending_size + bytes + 1);
    if (!next) { ctx->status = ATENA_ERR_NO_MEMORY; return 0; }
    ctx->pending = next;
    memcpy(ctx->pending + ctx->pending_size, data, bytes);
    ctx->pending_size += bytes;
    ctx->pending[ctx->pending_size] = '\0';

    size_t consumed = 0;
    for (size_t i = 0; i < ctx->pending_size; ++i) {
        if (ctx->pending[i] != '\n') continue;
        if (!process_line(ctx, ctx->pending + consumed, i - consumed)) return 0;
        consumed = i + 1;
    }
    if (consumed) {
        memmove(ctx->pending, ctx->pending + consumed, ctx->pending_size - consumed);
        ctx->pending_size -= consumed;
        ctx->pending[ctx->pending_size] = '\0';
    }
    return bytes;
}

static int progress_callback(void *userdata, curl_off_t a, curl_off_t b, curl_off_t c, curl_off_t d) {
    (void)a; (void)b; (void)c; (void)d;
    SseContext *ctx = userdata;
    return is_cancelled(ctx->impl) ? 1 : 0;
}

static const char *reasoning_effort_name(AtenaReasoningLevel level) {
    switch (level) {
        case ATENA_REASONING_DISABLED: return "none";
        case ATENA_REASONING_LOW: return "low";
        case ATENA_REASONING_MEDIUM: return "medium";
        case ATENA_REASONING_HIGH: return "high";
        default: return NULL;
    }
}

static void apply_openai_compatible_reasoning(AtenaProvider *provider, const AtenaProviderRequest *request, json_object *root) {
    if (!provider || !request || !root || request->reasoning == ATENA_REASONING_AUTO) return;
    const char *effort = reasoning_effort_name(request->reasoning);
    if (!effort) return;
    if (!strcmp(provider->id,"openai") || !strcmp(provider->id,"gemini") || !strcmp(provider->id,"xai")) {
        json_object_object_add(root,"reasoning_effort",json_object_new_string(effort));
    } else if (!strcmp(provider->id,"deepseek")) {
        json_object *thinking=json_object_new_object();
        json_object_object_add(thinking,"type",json_object_new_string(request->reasoning==ATENA_REASONING_DISABLED?"disabled":"enabled"));
        json_object_object_add(root,"thinking",thinking);
        json_object_object_add(root,"reasoning_effort",json_object_new_string(effort));
    }
}

static AtenaStatus openai_generate(AtenaProvider *provider, const AtenaProviderRequest *request,
                                   AtenaEventCallback callback, void *userdata) {
    if (!provider || !request || !callback || !request->messages || !request->message_count) return ATENA_ERR_INVALID_ARGUMENT;
    if ((!request->model || !*request->model) && !provider->model[0]) return ATENA_ERR_PROVIDER_INVALID;
    OpenAiCompatImpl *impl = provider->impl;
    atena_once(&curl_once, initialize_curl);

    atena_mutex_lock(&impl->lock);
    impl->cancelled = 0;
    snprintf(impl->active_operation, sizeof(impl->active_operation), "%s", request->operation_id ? request->operation_id : "");
    atena_mutex_unlock(&impl->lock);

    json_object *root = json_object_new_object();
    json_object *messages = json_object_new_array();
    if (!root || !messages) { if (root) json_object_put(root); if (messages) json_object_put(messages); return ATENA_ERR_NO_MEMORY; }
    json_object_object_add(root, "model", json_object_new_string(request->model && *request->model ? request->model : provider->model));
    json_object_object_add(root, "stream", json_object_new_boolean(1));
    json_object *stream_options = json_object_new_object();
    json_object_object_add(stream_options, "include_usage", json_object_new_boolean(1));
    json_object_object_add(root, "stream_options", stream_options);
    /* Reasoning models often reject or ignore sampling controls. Keep them for
       ordinary turns and let explicit reasoning use provider-native effort. */
    if (request->reasoning == ATENA_REASONING_AUTO || request->reasoning == ATENA_REASONING_DISABLED) {
        json_object_object_add(root, "temperature", json_object_new_double(request->temperature));
        json_object_object_add(root, "top_p", json_object_new_double(request->top_p));
    }
    if (request->max_output_tokens) {
        const char *limit_key = !strcmp(provider->id,"openai") ? "max_completion_tokens" : "max_tokens";
        json_object_object_add(root, limit_key, json_object_new_int64((int64_t)request->max_output_tokens));
    }
    apply_openai_compatible_reasoning(provider, request, root);
    for (size_t i = 0; i < request->message_count; ++i) {
        json_object *m = json_object_new_object();
        json_object_object_add(m, "role", json_object_new_string(role_name(request->messages[i].role)));
        json_object_object_add(m, "content", json_object_new_string(request->messages[i].content ? request->messages[i].content : ""));
        json_object_array_add(messages, m);
    }
    json_object_object_add(root, "messages", messages);

    char url[1024];
    if (!build_url(impl, "/chat/completions", url, sizeof(url))) { json_object_put(root); return ATENA_ERR_INVALID_ARGUMENT; }
    const char *body = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    CURL *curl = curl_easy_init();
    if (!curl) { json_object_put(root); return ATENA_ERR_INTERNAL; }
    struct curl_slist *headers = make_headers(impl);
    SseContext ctx = {impl, request->operation_id, callback, userdata, NULL, 0, 0, 0, 0, ATENA_OK};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(body));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive_sse);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 8000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, OPENAI_COMPAT_DEFAULT_TIMEOUT_MS);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 180L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);

    CURLcode result = curl_easy_perform(curl);
    long http = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    if (ctx.pending_size && ctx.status == ATENA_OK) (void)process_line(&ctx, ctx.pending, ctx.pending_size);
    free(ctx.pending);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    json_object_put(root);

    atena_mutex_lock(&impl->lock);
    impl->active_operation[0] = '\0';
    atena_mutex_unlock(&impl->lock);

    if (ctx.status != ATENA_OK) return ctx.status;
    if (result == CURLE_OPERATION_TIMEDOUT) return ATENA_ERR_TIMEOUT;
    if (result == CURLE_ABORTED_BY_CALLBACK) return ATENA_ERR_CANCELLED;
    if (result != CURLE_OK) return ATENA_ERR_NETWORK;
    if (http == 401 || http == 403) return ATENA_ERR_PERMISSION;
    if (http < 200 || http >= 300) return ATENA_ERR_PROVIDER_INVALID;
    if (ctx.text_bytes == 0) return ATENA_ERR_PROVIDER_INVALID;
    return ATENA_OK;
}

static AtenaStatus openai_cancel(AtenaProvider *provider, const char *operation_id) {
    if (!provider || !operation_id) return ATENA_ERR_INVALID_ARGUMENT;
    OpenAiCompatImpl *impl = provider->impl;
    atena_mutex_lock(&impl->lock);
    if (!strcmp(impl->active_operation, operation_id)) impl->cancelled = 1;
    atena_mutex_unlock(&impl->lock);
    return ATENA_OK;
}

static void openai_destroy(AtenaProvider *provider) {
    if (!provider) return;
    OpenAiCompatImpl *impl = provider->impl;
    if (impl) {
        atena_mutex_destroy(&impl->lock);
        memset(impl->bearer_token, 0, sizeof(impl->bearer_token));
    }
    free(impl);
    free(provider);
}

static const AtenaProviderVTable OPENAI_COMPAT_VTABLE = {openai_generate, openai_cancel, openai_destroy};

AtenaProvider *atena_openai_compatible_provider_create(const char *id, const char *base_url,
                                                        const char *model, const char *token) {
    if (!id || !*id || !base_url || !*base_url) return NULL;
    AtenaProvider *provider = calloc(1, sizeof(*provider));
    OpenAiCompatImpl *impl = calloc(1, sizeof(*impl));
    if (!provider || !impl) { free(provider); free(impl); return NULL; }
    snprintf(provider->id, sizeof(provider->id), "%s", id);
    snprintf(provider->type, sizeof(provider->type), "openai_compatible");
    snprintf(provider->model, sizeof(provider->model), "%s", model ? model : "");
    snprintf(impl->base_url, sizeof(impl->base_url), "%s", base_url);
    size_t len = strlen(impl->base_url);
    while (len > 0 && impl->base_url[len - 1] == '/') impl->base_url[--len] = '\0';
    snprintf(impl->bearer_token, sizeof(impl->bearer_token), "%s", token ? token : "");
    atena_mutex_init(&impl->lock);
    provider->capabilities = ATENA_CAP_TEXT | ATENA_CAP_STREAMING | ATENA_CAP_MODEL_LIST |
                             ATENA_CAP_USAGE | ATENA_CAP_CANCELLATION;
    if (!strcmp(id,"openai") || !strcmp(id,"gemini") || !strcmp(id,"deepseek") || !strcmp(id,"xai"))
        provider->capabilities |= ATENA_CAP_REASONING_CONTROL;
    provider->capabilities_known = provider->capabilities;
    provider->vtable = &OPENAI_COMPAT_VTABLE;
    provider->impl = impl;
    return provider;
}

AtenaStatus atena_openai_compatible_provider_test_json(AtenaProvider *provider, char **out_json) {
    if (!provider || !out_json || strcmp(provider->type, "openai_compatible") != 0) return ATENA_ERR_INVALID_ARGUMENT;
    *out_json = NULL;
    char *raw = NULL;
    AtenaStatus status = http_get(provider->impl, "/models", 20000L, &raw);
    if (status != ATENA_OK) return status;
    json_object *models = json_tokener_parse(raw);
    free(raw);
    if (!models) return ATENA_ERR_JSON;
    json_object *result = json_object_new_object();
    json_object_object_add(result, "ok", json_object_new_boolean(1));
    json_object_object_add(result, "provider_id", json_object_new_string(provider->id));
    json_object_object_add(result, "message", json_object_new_string("API OpenAI-compatible conectada"));
    *out_json = strdup(json_object_to_json_string_ext(result, JSON_C_TO_STRING_PLAIN));
    json_object_put(result);
    json_object_put(models);
    return *out_json ? ATENA_OK : ATENA_ERR_NO_MEMORY;
}

AtenaStatus atena_openai_compatible_provider_models_json(AtenaProvider *provider, char **out_json) {
    if (!provider || !out_json || strcmp(provider->type, "openai_compatible") != 0) return ATENA_ERR_INVALID_ARGUMENT;
    *out_json = NULL;
    char *raw = NULL;
    AtenaStatus status = http_get(provider->impl, "/models", 30000L, &raw);
    if (status != ATENA_OK) return status;
    json_object *remote = json_tokener_parse(raw);
    free(raw);
    if (!remote) return ATENA_ERR_JSON;
    json_object *data = NULL;
    if (!json_object_object_get_ex(remote, "data", &data) || !json_object_is_type(data, json_type_array)) {
        json_object_put(remote);
        return ATENA_ERR_PROTOCOL;
    }
    json_object *items = json_object_new_array();
    size_t count = json_object_array_length(data);
    for (size_t i = 0; i < count; ++i) {
        json_object *src = json_object_array_get_idx(data, i), *v = NULL;
        if (!src || !json_object_object_get_ex(src, "id", &v) || !json_object_is_type(v, json_type_string)) continue;
        const char *id = json_object_get_string(v);
        json_object *item = json_object_new_object();
        json_object_object_add(item, "id", json_object_new_string(id));
        json_object_object_add(item, "name", json_object_new_string(id));
        json_object_object_add(item, "provider_id", json_object_new_string(provider->id));
        json_object_object_add(item, "runtime", json_object_new_string("OpenAI-compatible"));
        json_object_object_add(item, "size", json_object_new_string("remoto"));
        json_object_object_add(item, "size_bytes", json_object_new_int64(0));
        json_object_object_add(item, "active", json_object_new_boolean(provider->model[0] && strcmp(id, provider->model) == 0));
        json_object_array_add(items, item);
    }
    *out_json = strdup(json_object_to_json_string_ext(items, JSON_C_TO_STRING_PLAIN));
    json_object_put(items);
    json_object_put(remote);
    return *out_json ? ATENA_OK : ATENA_ERR_NO_MEMORY;
}
