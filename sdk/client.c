#include "atena/client.h"
#include "atena/path.h"
#include "atena/platform.h"
#include "../ipc/protocol.h"
#include "../core/util.h"

#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

struct AtenaClient {
    AtenaPaths paths;
    char endpoint[ATENA_PATH_MAX];
    int connect_timeout_ms;
    int request_timeout_ms;
};

static int ipc_debug_enabled(void) {
    const char *v=getenv("ATENA_DEBUG_IPC");
    return v && *v && strcmp(v,"0")!=0;
}

static AtenaStatus open_connection(AtenaClient *client, AtenaTransport *transport) {
    return atena_transport_connect(client->endpoint, client->connect_timeout_ms, transport);
}

static json_object *make_request(const char *method, json_object *params, const char *idempotency_key, char id[37]) {
    if (!atena_uuid4(id)) return NULL;
    json_object *root = json_object_new_object();
    if (!root) return NULL;
    json_object_object_add(root, "protocol", json_object_new_string("atena.ipc/2"));
    json_object_object_add(root, "type", json_object_new_string("request"));
    json_object_object_add(root, "id", json_object_new_string(id));
    json_object_object_add(root, "method", json_object_new_string(method));
    json_object_object_add(root, "params", params ? json_object_get(params) : json_object_new_object());
    if (idempotency_key && *idempotency_key)
        json_object_object_add(root, "idempotency_key", json_object_new_string(idempotency_key));
    return root;
}

static AtenaStatus response_status(json_object *response, json_object **out_result) {
    json_object *value = NULL;
    const char *protocol = NULL, *type = NULL;
    if (json_object_object_get_ex(response, "protocol", &value)) protocol = json_object_get_string(value);
    if (!protocol || strcmp(protocol, "atena.ipc/2") != 0) return ATENA_ERR_PROTOCOL;
    if (json_object_object_get_ex(response, "type", &value)) type = json_object_get_string(value);
    if (!type || strcmp(type, "response") != 0) return ATENA_ERR_PROTOCOL;
    int ok = 0;
    if (json_object_object_get_ex(response, "ok", &value)) ok = json_object_get_boolean(value);
    if (ok) {
        if (out_result && json_object_object_get_ex(response, "result", &value)) *out_result = value;
        return ATENA_OK;
    }
    json_object *error = NULL;
    if (!json_object_object_get_ex(response, "error", &error)) return ATENA_ERR_INTERNAL;
    int numeric = ATENA_ERR_INTERNAL;
    if (json_object_object_get_ex(error, "status", &value)) numeric = json_object_get_int(value);
    return (numeric >= ATENA_ERR_INVALID_ARGUMENT && numeric <= ATENA_ERR_PERMISSION)
        ? (AtenaStatus)numeric : ATENA_ERR_INTERNAL;
}

static AtenaStatus one_call(AtenaClient *client, const char *method, json_object *params,
                            const char *idempotency_key, char **out_json) {
    if (!client || !method || !out_json) return ATENA_ERR_INVALID_ARGUMENT;
    *out_json = NULL;
    AtenaTransport transport;
    AtenaStatus status = open_connection(client, &transport);
    if (status != ATENA_OK) return status;
    AtenaNativeHandle fd = atena_transport_native_handle(&transport);
    char id[37];
    json_object *request = make_request(method, params, idempotency_key, id);
    if (!request) { atena_transport_close(&transport); return ATENA_ERR_NO_MEMORY; }
    status = atena_ipc_write_frame(fd, json_object_to_json_string_ext(request, JSON_C_TO_STRING_PLAIN));
    json_object_put(request);
    if (status != ATENA_OK) { atena_transport_close(&transport); return status; }
    char *raw = NULL;
    status = atena_ipc_read_frame(fd, &raw, 4U * 1024U * 1024U);
    atena_transport_close(&transport);
    if (status != ATENA_OK) return status;
    json_object *response = json_tokener_parse(raw);
    free(raw);
    if (!response) return ATENA_ERR_JSON;
    json_object *result = NULL;
    status = response_status(response, &result);
    if (status == ATENA_OK)
        *out_json = atena_strdup(result ? json_object_to_json_string_ext(result, JSON_C_TO_STRING_PLAIN) : "null");
    json_object_put(response);
    return status == ATENA_OK && !*out_json ? ATENA_ERR_NO_MEMORY : status;
}

AtenaStatus atena_client_connect(const AtenaClientConfig *config, AtenaClient **out_client) {
    if (!config || !out_client) return ATENA_ERR_INVALID_ARGUMENT;
    *out_client = NULL;
    AtenaClient *client = calloc(1, sizeof(*client));
    if (!client) return ATENA_ERR_NO_MEMORY;
    AtenaStatus status = atena_paths_resolve(&client->paths);
    if (status != ATENA_OK) { free(client); return status; }
    const char *endpoint = config->endpoint && *config->endpoint ? config->endpoint : client->paths.endpoint;
    if (strlen(endpoint) >= sizeof(client->endpoint)) { free(client); return ATENA_ERR_INVALID_ARGUMENT; }
    snprintf(client->endpoint, sizeof(client->endpoint), "%s", endpoint);
    client->connect_timeout_ms = config->connect_timeout_ms > 0 ? config->connect_timeout_ms : 2000;
    client->request_timeout_ms = config->request_timeout_ms > 0 ? config->request_timeout_ms : 600000;
    AtenaTransport probe;
    status = open_connection(client, &probe);
    if (status != ATENA_OK) { free(client); return status; }
    atena_transport_close(&probe);
    *out_client = client;
    return ATENA_OK;
}

AtenaStatus atena_client_connect_or_start(const AtenaClientConfig *config, AtenaClient **out_client) {
    if (!config || !out_client) return ATENA_ERR_INVALID_ARGUMENT;
    *out_client = NULL;
    AtenaStatus status = atena_client_connect(config, out_client);
    if (status == ATENA_OK) {
        /* A transport connection alone is not enough. Validate that the peer is
         * actually an Atena Core speaking the expected IPC protocol. */
        char *hello = NULL;
        AtenaStatus hello_status = atena_client_hello(*out_client, &hello);
        free(hello);
        if (hello_status == ATENA_OK) return ATENA_OK;
        atena_client_close(*out_client);
        *out_client = NULL;
        if (config->endpoint && *config->endpoint) return hello_status;
        status = hello_status;
    } else if (config->endpoint && *config->endpoint) {
        return status;
    }

    AtenaPaths paths;
    if ((status = atena_paths_resolve(&paths)) != ATENA_OK ||
        (status = atena_paths_prepare(&paths)) != ATENA_OK) return status;

#ifdef _WIN32
    HANDLE lock = CreateMutexA(NULL, FALSE, "Local\\Atena-Startup-v2");
    if (!lock) return ATENA_ERR_IO;
    DWORD wr = WaitForSingleObject(lock, 30000);
    if (wr != WAIT_OBJECT_0 && wr != WAIT_ABANDONED) { CloseHandle(lock); return ATENA_ERR_TIMEOUT; }
#else
    int lock_fd = open(paths.startup_lock, O_CREAT | O_RDWR, 0600);
    if (lock_fd < 0 || flock(lock_fd, LOCK_EX) != 0) { if (lock_fd >= 0) close(lock_fd); return ATENA_ERR_IO; }
#endif

    status = atena_client_connect(config, out_client);
    int spawned_core = 0;
    if (status != ATENA_OK) {
        status = atena_process_start_core(&paths);
        spawned_core = status == ATENA_OK;
    }

    if (status == ATENA_OK && !*out_client) {
        AtenaStatus last_connect_status = ATENA_ERR_NETWORK;
        for (int attempt = 0; attempt < 100; ++attempt) {
#ifdef _WIN32
            Sleep(100);
#else
            struct timespec delay = {0, 100000000L};
            nanosleep(&delay, NULL);
#endif
            last_connect_status = atena_client_connect(config, out_client);
            if (last_connect_status == ATENA_OK) {
                status = ATENA_OK;
                break;
            }
        }
        if (!*out_client) {
            /* A successful spawn followed by ten seconds without an IPC socket means
             * the child did not become ready (usually an early Core failure). Report
             * timeout instead of a misleading generic network error; stderr/stdout
             * are persisted by platform/process.c for diagnostics. */
            status = spawned_core ? ATENA_ERR_TIMEOUT : last_connect_status;
        }
    }

#ifdef _WIN32
    ReleaseMutex(lock); CloseHandle(lock);
#else
    (void)flock(lock_fd, LOCK_UN); close(lock_fd);
#endif

    if (status != ATENA_OK) return status;
    char *hello = NULL;
    status = atena_client_hello(*out_client, &hello);
    free(hello);
    if (status != ATENA_OK) { atena_client_close(*out_client); *out_client = NULL; }
    return status;
}

void atena_client_close(AtenaClient *client) { free(client); }
AtenaStatus atena_client_call(AtenaClient *c, const char *method, const char *params_json, char **out_json) {
    json_object *params = params_json ? json_tokener_parse(params_json) : json_object_new_object();
    if (!params || !json_object_is_type(params, json_type_object)) { if (params) json_object_put(params); return ATENA_ERR_JSON; }
    AtenaStatus status = one_call(c, method, params, NULL, out_json);
    json_object_put(params);
    return status;
}
AtenaStatus atena_client_hello(AtenaClient *c, char **out_json) { return one_call(c, "system.hello", NULL, NULL, out_json); }
AtenaStatus atena_client_status(AtenaClient *c, char **out_json) { return one_call(c, "system.status", NULL, NULL, out_json); }
AtenaStatus atena_client_doctor(AtenaClient *c, char **out_json) { return one_call(c, "system.doctor", NULL, NULL, out_json); }

AtenaStatus atena_client_session_create(AtenaClient *c, const char *title, char out_session_id[37]) {
    if (!c || !out_session_id) return ATENA_ERR_INVALID_ARGUMENT;
    json_object *params = json_object_new_object();
    json_object_object_add(params, "title", json_object_new_string(title ? title : "Nova conversa"));
    char *raw = NULL; AtenaStatus status = one_call(c, "sessions.create", params, NULL, &raw);
    json_object_put(params);
    if (status != ATENA_OK) return status;
    json_object *result = json_tokener_parse(raw); free(raw);
    json_object *value = NULL;
    if (!result || !json_object_object_get_ex(result, "session_id", &value)) { if (result) json_object_put(result); return ATENA_ERR_PROTOCOL; }
    snprintf(out_session_id, 37, "%s", json_object_get_string(value));
    json_object_put(result); return ATENA_OK;
}

static int parse_event(const char *name, AtenaEventType *out_type) {
    if (!name || !out_type) return 0;
    if (!strcmp(name,"operation.started")) *out_type=ATENA_EVENT_START;
    else if (!strcmp(name,"chat.delta")) *out_type=ATENA_EVENT_TEXT_DELTA;
    else if (!strcmp(name,"chat.reasoning_delta")) *out_type=ATENA_EVENT_REASONING_DELTA;
    else if (!strcmp(name,"tool.proposed")) *out_type=ATENA_EVENT_TOOL_CALL;
    else if (!strcmp(name,"tool.result")) *out_type=ATENA_EVENT_TOOL_RESULT;
    else if (!strcmp(name,"chat.citations")) *out_type=ATENA_EVENT_CITATION;
    else if (!strcmp(name,"operation.usage")) *out_type=ATENA_EVENT_USAGE;
    else if (!strcmp(name,"operation.finished")) *out_type=ATENA_EVENT_DONE;
    else return 0; /* Forward-compatible event: ignore rather than fail protocol. */
    return 1;
}

AtenaStatus atena_client_chat_send_ex(AtenaClient *c, const char *session_id, const char *provider_id,
                                      const char *text, AtenaReasoningLevel reasoning,
                                      AtenaEventCallback callback, void *userdata,
                                      char out_operation_id[37]) {
    if (!c || !session_id || !text || !out_operation_id) return ATENA_ERR_INVALID_ARGUMENT;
    out_operation_id[0] = '\0';
    AtenaStatus terminal_status = ATENA_OK;
    AtenaTransport transport; AtenaStatus status = open_connection(c, &transport);
    if (status != ATENA_OK) return status;
    AtenaNativeHandle fd = atena_transport_native_handle(&transport);
    json_object *params = json_object_new_object();
    json_object_object_add(params,"session_id",json_object_new_string(session_id));
    if (provider_id && *provider_id) json_object_object_add(params,"provider_id",json_object_new_string(provider_id));
    json_object_object_add(params,"text",json_object_new_string(text));
    const char *model_override=getenv("ATENA_MODEL_OVERRIDE");
    if(model_override&&*model_override)json_object_object_add(params,"model",json_object_new_string(model_override));
    json_object_object_add(params,"use_rag",json_object_new_boolean(1));
    json_object_object_add(params,"reasoning",json_object_new_int((int)reasoning));
    char id[37]; json_object *request = make_request("chat.start", params, NULL, id); json_object_put(params);
    if (!request) { atena_transport_close(&transport); return ATENA_ERR_NO_MEMORY; }
    status = atena_ipc_write_frame(fd,json_object_to_json_string_ext(request,JSON_C_TO_STRING_PLAIN)); json_object_put(request);
    if (status != ATENA_OK) { atena_transport_close(&transport); return status; }
    for (;;) {
        char *raw = NULL; status = atena_ipc_read_frame(fd,&raw,4U*1024U*1024U);
        if (status != ATENA_OK) { atena_transport_close(&transport); return status; }
        if (ipc_debug_enabled()) fprintf(stderr,"ATENA IPC RX: %s\n",raw);
        json_object *message = json_tokener_parse(raw); free(raw);
        if (!message) { atena_transport_close(&transport); return ATENA_ERR_JSON; }
        json_object *value = NULL; const char *type = NULL;
        if (json_object_object_get_ex(message,"type",&value)) type=json_object_get_string(value);
        if (type && !strcmp(type,"event")) {
            AtenaStreamEvent event = {0}; const char *name = NULL; json_object *payload = NULL;
            const char *request_id = NULL;
            if (json_object_object_get_ex(message,"request_id",&value) && json_object_is_type(value,json_type_string))
                request_id=json_object_get_string(value);
            if (request_id && strcmp(request_id,id)!=0) { if(ipc_debug_enabled())fprintf(stderr,"ATENA IPC protocol: request_id mismatch expected=%s got=%s\n",id,request_id); json_object_put(message); atena_transport_close(&transport); return ATENA_ERR_PROTOCOL; }
            if (json_object_object_get_ex(message,"event",&value)) name=json_object_get_string(value);
            if (!parse_event(name,&event.type)) { json_object_put(message); continue; }
            if (json_object_object_get_ex(message,"operation_id",&value)) {
                event.operation_id=json_object_get_string(value);
                if(out_operation_id[0] && event.operation_id && strcmp(out_operation_id,event.operation_id)!=0) {
                    if(ipc_debug_enabled())
                        fprintf(stderr,"ATENA IPC protocol: operation_id mismatch expected=%s got=%s\n",
                                out_operation_id,event.operation_id?event.operation_id:"");
                    json_object_put(message);
                    atena_transport_close(&transport);
                    return ATENA_ERR_PROTOCOL;
                }
                if(!out_operation_id[0] && event.operation_id) snprintf(out_operation_id,37,"%s",event.operation_id);
            }
            if (json_object_object_get_ex(message,"sequence",&value)) event.seq=(uint64_t)json_object_get_int64(value);
            json_object_object_get_ex(message,"payload",&payload);
            if (payload && name && !strcmp(name,"operation.finished")) {
                json_object *status_value=NULL,*error_value=NULL;
                const char *finished_status=NULL;
                if(json_object_object_get_ex(payload,"status",&status_value)&&json_object_is_type(status_value,json_type_string))finished_status=json_object_get_string(status_value);
                if(finished_status&&!strcmp(finished_status,"failed")){
                    event.type=ATENA_EVENT_ERROR;
                    int numeric=ATENA_ERR_PROVIDER_INVALID;
                    if(json_object_object_get_ex(payload,"error_code",&error_value))numeric=json_object_get_int(error_value);
                    event.error_code=numeric;
                    terminal_status=(numeric>=ATENA_ERR_INVALID_ARGUMENT&&numeric<=ATENA_ERR_PERMISSION)?(AtenaStatus)numeric:ATENA_ERR_PROVIDER_INVALID;
                } else if(finished_status&&!strcmp(finished_status,"cancelled")) {
                    event.type=ATENA_EVENT_ERROR; event.error_code=ATENA_ERR_CANCELLED; terminal_status=ATENA_ERR_CANCELLED;
                }
            }
            if (payload && json_object_object_get_ex(payload,"text",&value)) event.text=json_object_get_string(value);
            if (payload && json_object_object_get_ex(payload,"tool_name",&value)) event.tool_name=json_object_get_string(value);
            if (payload && json_object_object_get_ex(payload,"arguments",&value)) event.tool_json=json_object_to_json_string_ext(value,JSON_C_TO_STRING_PLAIN);
            int stop = callback ? callback(&event,userdata) : 0; json_object_put(message);
            if (stop) { atena_transport_close(&transport); return ATENA_ERR_CANCELLED; }
            continue;
        }
        if (type && !strcmp(type,"response")) {
            const char *response_id=NULL;
            if (json_object_object_get_ex(message,"id",&value) && json_object_is_type(value,json_type_string))
                response_id=json_object_get_string(value);
            if (!response_id || strcmp(response_id,id)!=0) { if(ipc_debug_enabled())fprintf(stderr,"ATENA IPC protocol: response id mismatch expected=%s got=%s\n",id,response_id?response_id:"(null)"); json_object_put(message); atena_transport_close(&transport); return ATENA_ERR_PROTOCOL; }
            json_object *result = NULL; status=response_status(message,&result);
            if (status==ATENA_OK && result && json_object_object_get_ex(result,"operation_id",&value)) {
                const char *response_op=json_object_get_string(value);
                if (out_operation_id[0] && response_op && strcmp(out_operation_id,response_op)!=0) status=ATENA_ERR_PROTOCOL;
                else if (!out_operation_id[0] && response_op) snprintf(out_operation_id,37,"%s",response_op);
            }
            if(status==ATENA_OK&&terminal_status!=ATENA_OK)status=terminal_status;
            json_object_put(message); atena_transport_close(&transport); return status;
        }
        if(ipc_debug_enabled())fprintf(stderr,"ATENA IPC protocol: unknown frame type=%s\n",type?type:"(null)");
        json_object_put(message); atena_transport_close(&transport); return ATENA_ERR_PROTOCOL;
    }
}

AtenaStatus atena_client_chat_send(AtenaClient *c, const char *session_id, const char *provider_id,
                                   const char *text, AtenaEventCallback callback, void *userdata,
                                   char out_operation_id[37]) {
    return atena_client_chat_send_ex(c, session_id, provider_id, text, ATENA_REASONING_AUTO,
                                     callback, userdata, out_operation_id);
}

AtenaStatus atena_client_cancel(AtenaClient *c,const char *operation_id) {
    if (!c || !operation_id) return ATENA_ERR_INVALID_ARGUMENT;
    json_object *params=json_object_new_object(); json_object_object_add(params,"operation_id",json_object_new_string(operation_id));
    char *raw=NULL; AtenaStatus status=one_call(c,"operation.cancel",params,NULL,&raw); json_object_put(params); free(raw); return status;
}
void atena_client_free_string(char *value) { free(value); }
