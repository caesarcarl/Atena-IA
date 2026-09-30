#include "atena/core.h"
#include "atena/provider.h"
#include "test_common.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int create_intermediate_database(const char *path) {
    sqlite3 *db = NULL;
    if (sqlite3_open(path, &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return 0;
    }

    /* Reproduces the kind of partially migrated database seen in real installs:
     * - schema version already advanced
     * - messages still use conversation_id
     * - preferences has key/data/created_at instead of key/value/updated_at
     * - providers is missing several columns expected by current code. */
    const char *sql =
        "PRAGMA user_version=3;"
        "CREATE TABLE sessions("
        " id TEXT PRIMARY KEY,title TEXT NOT NULL DEFAULT '',created_at TEXT NOT NULL,updated_at TEXT NOT NULL);"
        "CREATE TABLE messages("
        " id TEXT PRIMARY KEY,conversation_id TEXT NOT NULL,role TEXT NOT NULL,content TEXT NOT NULL,created_at TEXT NOT NULL);"
        "INSERT INTO messages(id,conversation_id,role,content,created_at) VALUES"
        " ('m1','legacy-session','user','ola','2026-01-01T00:00:00Z');"
        "CREATE TABLE preferences(key TEXT PRIMARY KEY,data TEXT,created_at TEXT);"
        "INSERT INTO preferences(key,data,created_at) VALUES"
        " ('provider.ollama.model','legacy-model','2026-01-01T00:00:00Z');"
        "CREATE TABLE providers(id TEXT PRIMARY KEY,model TEXT);"
        "INSERT INTO providers(id,model) VALUES('old-provider','old-model');";

    char *error = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &error);
    if (error) sqlite3_free(error);
    sqlite3_close(db);
    return rc == SQLITE_OK;
}

static int has_column(sqlite3 *db, const char *table, const char *column) {
    char sql[160];
    snprintf(sql, sizeof(sql), "PRAGMA table_info(\"%s\")", table);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    int found = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        if (name && strcmp(name, column) == 0) { found = 1; break; }
    }
    sqlite3_finalize(stmt);
    return found;
}

int main(void) {
    char path[] = "/tmp/atena-intermediate-migration-XXXXXX";
    int fd = mkstemp(path);
    ATENA_TEST_ASSERT(fd >= 0);
    close(fd);
    unlink(path);
    ATENA_TEST_ASSERT(create_intermediate_database(path));

    AtenaCoreConfig config = {path, "identity", 12000, 2, 1};
    AtenaCore *core = NULL;
    ATENA_TEST_ASSERT(atena_core_create(&config, &core) == ATENA_OK);

    /* This is the exact startup path that previously returned database_error. */
    ATENA_TEST_ASSERT(atena_core_provider_configure(core, "ollama", "ollama", NULL, NULL, NULL) == ATENA_OK);

    AtenaMessage *messages = NULL;
    size_t count = 0;
    ATENA_TEST_ASSERT(atena_core_session_history(core, "legacy-session", &messages, &count) == ATENA_OK);
    ATENA_TEST_ASSERT(count == 1);
    ATENA_TEST_ASSERT(strcmp(messages[0].content, "ola") == 0);
    atena_core_messages_free(messages, count);
    atena_core_destroy(core);

    sqlite3 *db = NULL;
    ATENA_TEST_ASSERT(sqlite3_open(path, &db) == SQLITE_OK);
    ATENA_TEST_ASSERT(has_column(db, "messages", "session_id"));
    ATENA_TEST_ASSERT(has_column(db, "preferences", "value"));
    ATENA_TEST_ASSERT(has_column(db, "preferences", "updated_at"));
    ATENA_TEST_ASSERT(has_column(db, "providers", "type"));
    ATENA_TEST_ASSERT(has_column(db, "providers", "capabilities_known"));

    sqlite3_stmt *stmt = NULL;
    ATENA_TEST_ASSERT(sqlite3_prepare_v2(db,
        "SELECT value FROM preferences WHERE key='provider.ollama.model'",
        -1, &stmt, NULL) == SQLITE_OK);
    ATENA_TEST_ASSERT(sqlite3_step(stmt) == SQLITE_ROW);
    const char *value = (const char *)sqlite3_column_text(stmt, 0);
    ATENA_TEST_ASSERT(value && strcmp(value, "legacy-model") == 0);
    sqlite3_finalize(stmt);

    ATENA_TEST_ASSERT(sqlite3_prepare_v2(db, "PRAGMA user_version", -1, &stmt, NULL) == SQLITE_OK);
    ATENA_TEST_ASSERT(sqlite3_step(stmt) == SQLITE_ROW);
    ATENA_TEST_ASSERT(sqlite3_column_int(stmt, 0) == 5);
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    unlink(path);
    puts("test_migration_intermediate: PASS");
    return 0;
}
