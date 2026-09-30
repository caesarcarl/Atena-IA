#ifndef ATENA_CONTEXT_H
#define ATENA_CONTEXT_H

#include <stddef.h>
#include "atena/status.h"
#include "atena/types.h"
#include "../memory/store.h"

typedef struct AtenaContextConfig {
    const char *identity_dir;
    size_t budget_chars;
    size_t rag_limit;
} AtenaContextConfig;

typedef struct AtenaBuiltContext {
    AtenaMessage *messages;
    size_t message_count;
    AtenaRagHit *rag_hits;
    size_t rag_count;
    int history_truncated;
    int rag_truncated;
} AtenaBuiltContext;

AtenaStatus atena_context_build(AtenaStore *store,
                                const AtenaContextConfig *config,
                                const char *session_id,
                                const char *user_text,
                                int use_rag,
                                AtenaBuiltContext *out);
void atena_context_free(AtenaBuiltContext *ctx);
AtenaStatus atena_identity_effective_text(const char *identity_dir, char **out_text);

#endif
