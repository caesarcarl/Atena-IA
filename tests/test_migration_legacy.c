#include "atena/core.h"
#include "test_common.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int create_legacy_database(const char *path) {
    sqlite3 *db = NULL;
    if (sqlite3_open(path, &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return 0;
    }

    const char *sql =
        "PRAGMA user_version=0;"
        "CREATE TABLE messages("
        " id TEXT PRIMARY KEY,"
        " conversation_id TEXT NOT NULL,"
        " role TEXT NOT NULL,"
        " content TEXT NOT NULL,"
        " created_at TEXT NOT NULL);"
        "INSERT INTO messages(id,conversation_id,role,content,created_at) VALUES"
        " ('legacy-user','legacy-session','user','mensagem antiga','2026-01-01T00:00:00Z'),"
        " ('legacy-assistant','legacy-session','assistant','resposta antiga','2026-01-01T00:00:01Z');";

    char *error = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &error);
    if (error) sqlite3_free(error);
    sqlite3_close(db);
    return rc == SQLITE_OK;
}

int main(void) {
    char path[] = "/tmp/atena-legacy-migration-XXXXXX";
    int fd = mkstemp(path);
    ATENA_TEST_ASSERT(fd >= 0);
    close(fd);
    unlink(path);

    ATENA_TEST_ASSERT(create_legacy_database(path));

    AtenaCoreConfig config = {path, "identity", 12000, 2, 1};
    AtenaCore *core = NULL;
    ATENA_TEST_ASSERT(atena_core_create(&config, &core) == ATENA_OK);

    AtenaMessage *messages = NULL;
    size_t count = 0;
    ATENA_TEST_ASSERT(atena_core_session_history(core, "legacy-session", &messages, &count) == ATENA_OK);
    ATENA_TEST_ASSERT(count == 2);
    ATENA_TEST_ASSERT(messages[0].role == ATENA_ROLE_USER);
    ATENA_TEST_ASSERT(messages[0].state == ATENA_MSG_COMPLETE);
    ATENA_TEST_ASSERT(strcmp(messages[0].content, "mensagem antiga") == 0);
    ATENA_TEST_ASSERT(messages[1].role == ATENA_ROLE_ASSISTANT);
    ATENA_TEST_ASSERT(strcmp(messages[1].content, "resposta antiga") == 0);
    atena_core_messages_free(messages, count);

    char new_session[37];
    ATENA_TEST_ASSERT(atena_core_session_create(core, "nova sessão", new_session) == ATENA_OK);

    atena_core_destroy(core);
    unlink(path);
    puts("test_migration_legacy: PASS");
    return 0;
}
