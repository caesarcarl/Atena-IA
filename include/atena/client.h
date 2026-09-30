#ifndef ATENA_CLIENT_H
#define ATENA_CLIENT_H

#include <stddef.h>
#include "atena/status.h"
#include "atena/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AtenaClient AtenaClient;

typedef struct AtenaClientConfig {
    const char *endpoint;
    int connect_timeout_ms;
    int request_timeout_ms;
} AtenaClientConfig;

AtenaStatus atena_client_connect(const AtenaClientConfig *config, AtenaClient **out_client);
AtenaStatus atena_client_connect_or_start(const AtenaClientConfig *config, AtenaClient **out_client);
void atena_client_close(AtenaClient *client);
AtenaStatus atena_client_call(AtenaClient *client, const char *method, const char *params_json, char **out_json);
AtenaStatus atena_client_hello(AtenaClient *client, char **out_json);
AtenaStatus atena_client_status(AtenaClient *client, char **out_json);
AtenaStatus atena_client_doctor(AtenaClient *client, char **out_json);
AtenaStatus atena_client_session_create(AtenaClient *client, const char *title, char out_session_id[37]);
AtenaStatus atena_client_chat_send(AtenaClient *client,
                                   const char *session_id,
                                   const char *provider_id,
                                   const char *text,
                                   AtenaEventCallback callback,
                                   void *userdata,
                                   char out_operation_id[37]);
AtenaStatus atena_client_chat_send_ex(AtenaClient *client,
                                      const char *session_id,
                                      const char *provider_id,
                                      const char *text,
                                      AtenaReasoningLevel reasoning,
                                      AtenaEventCallback callback,
                                      void *userdata,
                                      char out_operation_id[37]);
AtenaStatus atena_client_cancel(AtenaClient *client, const char *operation_id);
void atena_client_free_string(char *value);

#ifdef __cplusplus
}
#endif
#endif
