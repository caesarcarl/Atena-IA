#include "server_dispatch.h"
#include "atena/secret.h"
#include "protocol.h"
#include "../tools/tools.h"

#include <json-c/json.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ATENA_PACKAGE_REVISION
#define ATENA_PACKAGE_REVISION "development"
#endif

/* This file deliberately contains the platform-independent IPC contract.
 * Unix sockets and Windows named pipes both call this dispatcher, preventing
 * the two platforms from silently drifting to different feature sets. */

typedef struct EventWriter {
    AtenaNativeHandle fd;
    const char *request_id;
    const char *session_id;
} EventWriter;

static const char *event_name(AtenaEventType type) {
    switch (type) {
        case ATENA_EVENT_START: return "operation.started";
        case ATENA_EVENT_TEXT_DELTA: return "chat.delta";
        case ATENA_EVENT_REASONING_DELTA: return "chat.reasoning_delta";
        case ATENA_EVENT_TOOL_CALL: return "tool.proposed";
        case ATENA_EVENT_TOOL_RESULT: return "tool.result";
        case ATENA_EVENT_CITATION: return "chat.citations";
        case ATENA_EVENT_USAGE: return "operation.usage";
        case ATENA_EVENT_ERROR: return "operation.finished";
        case ATENA_EVENT_DONE: return "operation.finished";
    }
    return "operation.warning";
}

static AtenaStatus write_json(AtenaNativeHandle fd, json_object *root) {
    const char *raw = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    if (getenv("ATENA_DEBUG_IPC")) fprintf(stderr, "ATENA IPC TX: %s\n", raw);
    return atena_ipc_write_frame(fd, raw);
}

static int write_event(const AtenaStreamEvent *event, void *userdata) {
    EventWriter *writer = (EventWriter *)userdata;
    json_object *root = json_object_new_object();
    json_object *payload = json_object_new_object();
    if (!root || !payload) {
        if (root) json_object_put(root);
        if (payload) json_object_put(payload);
        return 1;
    }

    json_object_object_add(root, "protocol", json_object_new_string("atena.ipc/2"));
    json_object_object_add(root, "type", json_object_new_string("event"));
    json_object_object_add(root, "request_id", json_object_new_string(writer->request_id ? writer->request_id : ""));
    json_object_object_add(root, "operation_id", json_object_new_string(event->operation_id ? event->operation_id : ""));
    if (writer->session_id) json_object_object_add(root, "session_id", json_object_new_string(writer->session_id));
    json_object_object_add(root, "event", json_object_new_string(event_name(event->type)));
    json_object_object_add(root, "sequence", json_object_new_int64((int64_t)event->seq));

    if (event->text) json_object_object_add(payload, "text", json_object_new_string(event->text));
    if (event->tool_name) json_object_object_add(payload, "tool_name", json_object_new_string(event->tool_name));
    if (event->tool_json) {
        json_object *arguments = json_tokener_parse(event->tool_json);
        json_object_object_add(payload, "arguments", arguments ? arguments : json_object_new_string(event->tool_json));
    }
    if (event->type == ATENA_EVENT_CITATION && event->citation_id) {
        json_object *items = json_object_new_array();
        json_object *item = json_object_new_object();
        json_object_object_add(item, "chunk_id", json_object_new_string(event->citation_id));
        json_object_object_add(item, "document_id", json_object_new_string(event->citation_document_id ? event->citation_document_id : ""));
        json_object_object_add(item, "title", json_object_new_string(event->citation_title ? event->citation_title : ""));
        json_object_object_add(item, "location", json_object_new_string(event->citation_locator ? event->citation_locator : ""));
        json_object_object_add(item, "content_sha256", json_object_new_string(event->citation_content_sha256 ? event->citation_content_sha256 : ""));
        json_object_array_add(items, item);
        json_object_object_add(payload, "items", items);
    }
    if (event->type == ATENA_EVENT_USAGE || event->type == ATENA_EVENT_DONE) {
        json_object *usage = json_object_new_object();
        json_object_object_add(usage, "prompt_tokens", json_object_new_int64((int64_t)event->metrics.prompt_tokens));
        json_object_object_add(usage, "completion_tokens", json_object_new_int64((int64_t)event->metrics.completion_tokens));
        json_object_object_add(payload, "usage", usage);
        json_object_object_add(payload, "total_ms", json_object_new_double(event->metrics.total_generation_ms));
    }
    if (event->type == ATENA_EVENT_ERROR) {
        json_object_object_add(payload, "status", json_object_new_string("failed"));
        json_object_object_add(payload, "error_code", json_object_new_int(event->error_code));
    } else if (event->type == ATENA_EVENT_DONE) {
        json_object_object_add(payload, "status", json_object_new_string("completed"));
    }

    json_object_object_add(root, "payload", payload);
    AtenaStatus status = write_json(writer->fd, root);
    json_object_put(root);
    return status == ATENA_OK ? 0 : 1;
}

static const char *error_code(AtenaStatus status) {
    switch (status) {
        case ATENA_ERR_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case ATENA_ERR_NOT_FOUND: return "NOT_FOUND";
        case ATENA_ERR_CONFLICT: return "IDEMPOTENCY_CONFLICT";
        case ATENA_ERR_PROVIDER_UNAVAILABLE: return "NOT_CONFIGURED";
        case ATENA_ERR_CANCELLED: return "CANCELLED";
        case ATENA_ERR_POLICY_DENIED: return "PERMISSION_DENIED";
        case ATENA_ERR_SCHEMA_TOO_NEW: return "SCHEMA_TOO_NEW";
        case ATENA_ERR_NETWORK: return "NETWORK_ERROR";
        case ATENA_ERR_TIMEOUT: return "TIMEOUT";
        case ATENA_ERR_PROTOCOL: return "PROTOCOL_MISMATCH";
        case ATENA_ERR_BUSY: return "BUSY";
        case ATENA_ERR_UNSUPPORTED: return "FEATURE_UNAVAILABLE";
        case ATENA_ERR_DB: return "DATABASE_ERROR";
        case ATENA_ERR_PATH: return "PATH_ERROR";
        case ATENA_ERR_PERMISSION: return "PERMISSION_ERROR";
        case ATENA_ERR_PROVIDER_INVALID: return "PROVIDER_ERROR";
        default: return "INTERNAL_ERROR";
    }
}

static AtenaStatus send_result(AtenaNativeHandle fd, const char *id, json_object *result) {
    json_object *root = json_object_new_object();
    json_object_object_add(root, "protocol", json_object_new_string("atena.ipc/2"));
    json_object_object_add(root, "type", json_object_new_string("response"));
    json_object_object_add(root, "id", json_object_new_string(id ? id : ""));
    json_object_object_add(root, "ok", json_object_new_boolean(1));
    json_object_object_add(root, "result", result ? json_object_get(result) : json_object_new_object());
    AtenaStatus status = write_json(fd, root);
    json_object_put(root);
    return status;
}

static AtenaStatus send_error(AtenaNativeHandle fd, const char *id, AtenaStatus status) {
    json_object *root = json_object_new_object();
    json_object *error = json_object_new_object();
    json_object_object_add(root, "protocol", json_object_new_string("atena.ipc/2"));
    json_object_object_add(root, "type", json_object_new_string("response"));
    json_object_object_add(root, "id", json_object_new_string(id ? id : ""));
    json_object_object_add(root, "ok", json_object_new_boolean(0));
    json_object_object_add(error, "code", json_object_new_string(error_code(status)));
    json_object_object_add(error, "message", json_object_new_string(atena_status_string(status)));
    json_object_object_add(error, "retryable", json_object_new_boolean(status == ATENA_ERR_NETWORK || status == ATENA_ERR_TIMEOUT || status == ATENA_ERR_BUSY));
    json_object_object_add(error, "status", json_object_new_int(status));
    json_object_object_add(error, "details", json_object_new_object());
    json_object_object_add(root, "error", error);
    AtenaStatus written = write_json(fd, root);
    json_object_put(root);
    return written;
}

static const char *jstr(json_object *o, const char *k) {
    json_object *v = NULL;
    return o && json_object_object_get_ex(o, k, &v) && json_object_is_type(v, json_type_string) ? json_object_get_string(v) : NULL;
}
static int jbool(json_object *o, const char *k, int d) {
    json_object *v = NULL;
    return o && json_object_object_get_ex(o, k, &v) ? json_object_get_boolean(v) : d;
}
static int jint(json_object *o, const char *k, int d) {
    json_object *v = NULL;
    return o && json_object_object_get_ex(o, k, &v) ? json_object_get_int(v) : d;
}

static json_object *methods_json(void) {
    static const char *methods[] = {
        "system.hello", "system.status", "system.doctor",
        "providers.list", "providers.put", "providers.test",
        "models.list", "models.select", "models.pull", "models.remove",
        "sessions.create", "sessions.messages", "chat.start", "operation.cancel",
        "tools.list", "rag.ingest", "documents.list", "identity.get", "memory.put"
    };
    json_object *array = json_object_new_array();
    for (size_t i = 0; i < sizeof(methods) / sizeof(methods[0]); ++i)
        json_object_array_add(array, json_object_new_string(methods[i]));
    return array;
}

AtenaStatus atena_ipc_dispatch_request(AtenaCore *core, AtenaNativeHandle fd, json_object *request) {
    if (!core || !request) return ATENA_ERR_INVALID_ARGUMENT;
    const char *id = jstr(request, "id");
    const char *method = jstr(request, "method");
    const char *protocol = jstr(request, "protocol");
    const char *type = jstr(request, "type");
    json_object *params = NULL;
    json_object_object_get_ex(request, "params", &params);

    if (!id || !method || !protocol || strcmp(protocol, "atena.ipc/2") || !type || strcmp(type, "request"))
        return send_error(fd, id, ATENA_ERR_PROTOCOL);

    if (!strcmp(method, "system.hello")) {
        json_object *r = json_object_new_object();
        json_object_object_add(r, "protocol", json_object_new_string("atena.ipc/2"));
        json_object_object_add(r, "name", json_object_new_string("Atena Core"));
        json_object_object_add(r, "version", json_object_new_string("0.5.0-base"));
        json_object_object_add(r, "package_revision", json_object_new_string(ATENA_PACKAGE_REVISION));
        json_object_object_add(r, "methods", methods_json());
        AtenaStatus s = send_result(fd, id, r); json_object_put(r); return s;
    }
    if (!strcmp(method, "system.status") || !strcmp(method, "system.doctor")) {
        char *raw = NULL; AtenaStatus s = atena_core_status_json(core, &raw);
        if (s != ATENA_OK) return send_error(fd, id, s);
        json_object *r = json_tokener_parse(raw); atena_core_free_string(raw);
        if (!r) return send_error(fd, id, ATENA_ERR_JSON);
        if (!strcmp(method, "system.doctor")) json_object_object_add(r, "diagnosis", json_object_new_string("core_ready"));
        s = send_result(fd, id, r); json_object_put(r); return s;
    }
    if (!strcmp(method, "providers.list")) {
        char *raw = NULL; AtenaStatus s = atena_core_status_json(core, &raw);
        if (s != ATENA_OK) return send_error(fd, id, s);
        json_object *status = json_tokener_parse(raw), *providers = NULL; atena_core_free_string(raw);
        if (!status) return send_error(fd, id, ATENA_ERR_JSON);
        json_object_object_get_ex(status, "providers", &providers);
        s = send_result(fd, id, providers); json_object_put(status); return s;
    }
    if (!strcmp(method, "providers.put")) {
        const char *provider_id = jstr(params, "provider_id");
        if (!provider_id) provider_id = jstr(params, "preset_id");
        if (!provider_id) provider_id = "ollama";
        const char *provider_type = jstr(params, "type");
        if (!provider_type) provider_type = provider_id;
        AtenaStatus s = atena_core_provider_configure(core, provider_id, provider_type,
                                                       jstr(params, "endpoint"), jstr(params, "model"), jstr(params, "secret_value"));
        if (s == ATENA_OK) s = atena_core_provider_select(core, provider_id);
        if (s != ATENA_OK) return send_error(fd, id, s);
        json_object *r = json_object_new_object();
        json_object_object_add(r, "provider_id", json_object_new_string(provider_id));
        json_object_object_add(r, "configured", json_object_new_boolean(1));
        int persisted = 0;
        const char *secret_value = jstr(params, "secret_value");
        if (secret_value && *secret_value && atena_secret_persistence_available()) {
            char *probe = NULL;
            if (atena_secret_lookup(provider_id, &probe) == ATENA_OK) { persisted = 1; atena_secret_free(probe); }
        }
        json_object_object_add(r, "secret_persisted", json_object_new_boolean(persisted));
        s = send_result(fd, id, r); json_object_put(r); return s;
    }
    if (!strcmp(method, "providers.test")) {
        const char *provider_id = jstr(params, "provider_id"); if (!provider_id) provider_id = "ollama";
        char *raw = NULL; AtenaStatus s = atena_core_provider_test_json(core, provider_id, &raw);
        if (s != ATENA_OK) return send_error(fd, id, s);
        json_object *r = json_tokener_parse(raw); atena_core_free_string(raw);
        if (!r) return send_error(fd, id, ATENA_ERR_JSON);
        s = send_result(fd, id, r); json_object_put(r); return s;
    }
    if (!strcmp(method, "models.list")) {
        const char *provider_id = jstr(params, "provider_id"); if (!provider_id) provider_id = "ollama";
        char *raw = NULL; AtenaStatus s = atena_core_models_list_json(core, provider_id, &raw);
        if (s != ATENA_OK) return send_error(fd, id, s);
        json_object *r = json_tokener_parse(raw); atena_core_free_string(raw);
        if (!r) return send_error(fd, id, ATENA_ERR_JSON);
        s = send_result(fd, id, r); json_object_put(r); return s;
    }
    if (!strcmp(method, "models.select")) {
        const char *provider_id = jstr(params, "provider_id"); if (!provider_id) provider_id = "ollama";
        AtenaStatus s = atena_core_model_select(core, provider_id, jstr(params, "model"));
        return s == ATENA_OK ? send_result(fd, id, NULL) : send_error(fd, id, s);
    }
    if (!strcmp(method, "models.pull")) {
        const char *provider_id = jstr(params, "provider_id"); if (!provider_id) provider_id = "ollama";
        AtenaStatus s = atena_core_model_pull(core, provider_id, jstr(params, "model"));
        return s == ATENA_OK ? send_result(fd, id, NULL) : send_error(fd, id, s);
    }
    if (!strcmp(method, "models.remove")) {
        const char *provider_id = jstr(params, "provider_id"); if (!provider_id) provider_id = "ollama";
        AtenaStatus s = atena_core_model_remove(core, provider_id, jstr(params, "model"));
        return s == ATENA_OK ? send_result(fd, id, NULL) : send_error(fd, id, s);
    }
    if (!strcmp(method, "sessions.create")) {
        char sid[37]; AtenaStatus s = atena_core_session_create(core, jstr(params, "title"), sid);
        if (s != ATENA_OK) return send_error(fd, id, s);
        json_object *r = json_object_new_object(); json_object_object_add(r, "session_id", json_object_new_string(sid));
        s = send_result(fd, id, r); json_object_put(r); return s;
    }
    if (!strcmp(method, "sessions.messages")) {
        AtenaMessage *m = NULL; size_t n = 0; AtenaStatus s = atena_core_session_history(core, jstr(params, "session_id"), &m, &n);
        if (s != ATENA_OK) return send_error(fd, id, s);
        json_object *a = json_object_new_array();
        for (size_t i = 0; i < n; ++i) {
            json_object *o = json_object_new_object();
            json_object_object_add(o, "id", json_object_new_string(m[i].id));
            json_object_object_add(o, "role", json_object_new_int(m[i].role));
            json_object_object_add(o, "state", json_object_new_int(m[i].state));
            json_object_object_add(o, "content", json_object_new_string(m[i].content));
            json_object_array_add(a, o);
        }
        atena_core_messages_free(m, n); s = send_result(fd, id, a); json_object_put(a); return s;
    }
    if (!strcmp(method, "chat.start")) {
        AtenaChatRequest r = {0};
        r.session_id = jstr(params, "session_id"); r.provider_id = jstr(params, "provider_id"); r.model = jstr(params, "model");
        r.user_text = jstr(params, "text"); r.idempotency_key = jstr(request, "idempotency_key"); r.use_rag = jbool(params, "use_rag", 1);
        r.max_output_tokens = (size_t)jint(params, "max_output_tokens", 512); r.reasoning = (AtenaReasoningLevel)jint(params, "reasoning", ATENA_REASONING_AUTO);
        char op[37] = {0}; EventWriter w = {fd, id, r.session_id}; AtenaStatus s = atena_core_chat_send(core, &r, write_event, &w, op);
        if (s != ATENA_OK && op[0] == '\0') return send_error(fd, id, s);
        json_object *result = json_object_new_object();
        json_object_object_add(result, "operation_id", json_object_new_string(op));
        json_object_object_add(result, "status", json_object_new_string(s == ATENA_OK ? "completed" : s == ATENA_ERR_CANCELLED ? "cancelled" : "failed"));
        AtenaStatus sent = send_result(fd, id, result); json_object_put(result); return sent;
    }
    if (!strcmp(method, "operation.cancel")) {
        AtenaStatus s = atena_core_chat_cancel(core, jstr(params, "operation_id"));
        return s == ATENA_OK ? send_result(fd, id, NULL) : send_error(fd, id, s);
    }
    if (!strcmp(method, "rag.ingest")) {
        char did[37]; AtenaStatus s = atena_core_rag_import_text(core, jstr(params, "title"), jstr(params, "locator"), jstr(params, "text"), did);
        if (s != ATENA_OK) return send_error(fd, id, s);
        json_object *r = json_object_new_object(); json_object_object_add(r, "document_id", json_object_new_string(did));
        s = send_result(fd, id, r); json_object_put(r); return s;
    }
    if (!strcmp(method, "documents.list")) {
        char *raw = NULL; AtenaStatus s = atena_core_documents_json(core, &raw);
        if (s != ATENA_OK) return send_error(fd, id, s);
        json_object *r = json_tokener_parse(raw); atena_core_free_string(raw);
        if (!r) return send_error(fd, id, ATENA_ERR_JSON);
        s = send_result(fd, id, r); json_object_put(r); return s;
    }
    if (!strcmp(method, "identity.get")) {
        char *raw = NULL; AtenaStatus s = atena_core_identity_json(core, &raw);
        if (s != ATENA_OK) return send_error(fd, id, s);
        json_object *r = json_tokener_parse(raw); atena_core_free_string(raw);
        if (!r) return send_error(fd, id, ATENA_ERR_JSON);
        s = send_result(fd, id, r); json_object_put(r); return s;
    }
    if (!strcmp(method, "memory.put")) {
        AtenaStatus s = atena_core_memory_put(core, jstr(params, "key"), jstr(params, "value"));
        return s == ATENA_OK ? send_result(fd, id, NULL) : send_error(fd, id, s);
    }
    if (!strcmp(method, "tools.list")) {
        char *raw = NULL; AtenaStatus s = atena_tool_list_json(&raw);
        if (s != ATENA_OK) return send_error(fd, id, s);
        json_object *r = json_tokener_parse(raw); free(raw);
        if (!r) return send_error(fd, id, ATENA_ERR_JSON);
        s = send_result(fd, id, r); json_object_put(r); return s;
    }
    return send_error(fd, id, ATENA_ERR_NOT_FOUND);
}
