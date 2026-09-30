#ifndef ATENA_MEMORY_STORE_H
#define ATENA_MEMORY_STORE_H

#include <stddef.h>
#include <sqlite3.h>
#include "atena/status.h"
#include "atena/types.h"

typedef struct AtenaStore {
    sqlite3 *db;
} AtenaStore;

typedef struct AtenaRagHit {
    char chunk_id[37];
    char document_id[37];
    char content_sha256[72];
    char *title;
    char *locator;
    char *content;
    double score;
} AtenaRagHit;

AtenaStatus atena_store_open(const char *path, AtenaStore **out_store);
void atena_store_close(AtenaStore *store);
AtenaStatus atena_store_session_create(AtenaStore *store, const char *title, char out_id[37]);
AtenaStatus atena_store_session_exists(AtenaStore *store, const char *session_id, int *out_exists);
AtenaStatus atena_store_message_add(AtenaStore *store, const AtenaMessage *message);
AtenaStatus atena_store_message_state(AtenaStore *store, const char *message_id, AtenaMessageState state);
AtenaStatus atena_store_history(AtenaStore *store, const char *session_id, AtenaMessage **out_messages, size_t *out_count);
AtenaStatus atena_store_preference_put(AtenaStore *store, const char *key, const char *value);
AtenaStatus atena_store_preference_get(AtenaStore *store, const char *key, char **out_value);
AtenaStatus atena_store_preferences_text(AtenaStore *store, char **out_text);
AtenaStatus atena_store_rag_import_text(AtenaStore *store, const char *title, const char *locator, const char *text, char out_document_id[37]);
AtenaStatus atena_store_documents_json(AtenaStore *store, char **out_json);
AtenaStatus atena_store_rag_search(AtenaStore *store, const char *query, size_t limit, AtenaRagHit **out_hits, size_t *out_count);
void atena_store_rag_hits_free(AtenaRagHit *hits, size_t count);
AtenaStatus atena_store_operation_begin(AtenaStore *store, const char *operation_id, const char *session_id, const char *idempotency_key);
AtenaStatus atena_store_operation_finish(AtenaStore *store, const char *operation_id, const char *state, int status_code);
AtenaStatus atena_store_operation_by_key(AtenaStore *store, const char *session_id, const char *idempotency_key, char out_operation_id[37], char out_state[24]);
AtenaStatus atena_store_provider_upsert(AtenaStore *store, const char *id, const char *type, const char *model, unsigned long long capabilities, unsigned long long capabilities_known, int enabled);
AtenaStatus atena_store_provider_list_json(AtenaStore *store, char **out_json);
AtenaStatus atena_store_audit(AtenaStore *store, const char *event, const char *session_id, const char *operation_id, const char *safe_details);

#endif
