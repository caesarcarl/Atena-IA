#ifndef ATENA_CORE_H
#define ATENA_CORE_H

#include <stddef.h>
#include "atena/status.h"
#include "atena/types.h"
#include "atena/provider.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AtenaCore AtenaCore;

typedef struct AtenaCoreConfig {
    const char *database_path;
    const char *identity_dir;
    size_t default_context_budget_chars;
    size_t default_rag_results;
    int offline_mode;
} AtenaCoreConfig;

typedef struct AtenaChatRequest {
    const char *session_id;
    const char *provider_id;
    const char *model;
    const char *user_text;
    const char *idempotency_key;
    int use_rag;
    size_t max_output_tokens;
    AtenaReasoningLevel reasoning;
} AtenaChatRequest;

AtenaStatus atena_core_create(const AtenaCoreConfig *config, AtenaCore **out_core);
void atena_core_destroy(AtenaCore *core);
AtenaStatus atena_core_register_provider(AtenaCore *core, AtenaProvider *provider);
AtenaStatus atena_core_provider_configure(AtenaCore *core,
                                          const char *provider_id,
                                          const char *provider_type,
                                          const char *endpoint,
                                          const char *model,
                                          const char *session_secret);
AtenaStatus atena_core_provider_select(AtenaCore *core, const char *provider_id);
AtenaStatus atena_core_restore_default_provider(AtenaCore *core);
AtenaStatus atena_core_provider_test_json(AtenaCore *core, const char *provider_id, char **out_json);
AtenaStatus atena_core_models_list_json(AtenaCore *core, const char *provider_id, char **out_json);
AtenaStatus atena_core_model_select(AtenaCore *core, const char *provider_id, const char *model);
AtenaStatus atena_core_model_pull(AtenaCore *core, const char *provider_id, const char *model);
AtenaStatus atena_core_model_remove(AtenaCore *core, const char *provider_id, const char *model);
AtenaStatus atena_core_session_create(AtenaCore *core, const char *title, char out_session_id[37]);
AtenaStatus atena_core_session_history(AtenaCore *core,
                                       const char *session_id,
                                       AtenaMessage **out_messages,
                                       size_t *out_count);
void atena_core_messages_free(AtenaMessage *messages, size_t count);
AtenaStatus atena_core_chat_send(AtenaCore *core,
                                 const AtenaChatRequest *request,
                                 AtenaEventCallback callback,
                                 void *userdata,
                                 char out_operation_id[37]);
AtenaStatus atena_core_chat_cancel(AtenaCore *core, const char *operation_id);
AtenaStatus atena_core_rag_import_text(AtenaCore *core,
                                       const char *title,
                                       const char *locator,
                                       const char *text,
                                       char out_document_id[37]);
AtenaStatus atena_core_documents_json(AtenaCore *core, char **out_json);
AtenaStatus atena_core_identity_json(AtenaCore *core, char **out_json);
AtenaStatus atena_core_memory_put(AtenaCore *core, const char *key, const char *value);
AtenaStatus atena_core_status_json(AtenaCore *core, char **out_json);
void atena_core_free_string(char *value);

#ifdef __cplusplus
}
#endif
#endif
