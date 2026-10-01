#include "../core/context.h"
#include "../memory/store.h"
#include "test_common.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    AtenaStore *store = NULL;
    ATENA_TEST_ASSERT(atena_store_open(":memory:", &store) == ATENA_OK);

    char session_id[37];
    ATENA_TEST_ASSERT(atena_store_session_create(store, "budget", session_id) == ATENA_OK);

    AtenaContextConfig config = {"identity", 1920, 1};
    AtenaBuiltContext ctx = {0};
    const char *question = "Responda somente: OK";

    ATENA_TEST_ASSERT(
        atena_context_build(store, &config, session_id, question, 0, &ctx) == ATENA_OK);
    ATENA_TEST_ASSERT(ctx.message_count >= 3);

    size_t total_chars = 0;
    int compact_identity_seen = 0;
    int user_seen = 0;
    for (size_t i = 0; i < ctx.message_count; ++i) {
        const char *content = ctx.messages[i].content ? ctx.messages[i].content : "";
        total_chars += strlen(content);
        if (strstr(content, "[ATENA IDENTITY COMPACT]"))
            compact_identity_seen = 1;
        if (ctx.messages[i].role == ATENA_ROLE_USER &&
            strcmp(content, question) == 0)
            user_seen = 1;
    }

    ATENA_TEST_ASSERT(compact_identity_seen);
    ATENA_TEST_ASSERT(user_seen);
    ATENA_TEST_ASSERT(total_chars <= config.budget_chars);

    atena_context_free(&ctx);
    atena_store_close(store);

    puts("test_context_budget: PASS");
    return 0;
}
