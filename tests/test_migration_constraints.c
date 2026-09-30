#include "atena/core.h"
#include "test_common.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int create_broken_constraint_database(const char *path) {
    sqlite3 *db = NULL;
    if (sqlite3_open(path, &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return 0;
    }
    /* Reproduces an installed DB whose columns look current but whose key/id
     * columns are not UNIQUE/PRIMARY KEY. ON CONFLICT(key/id) fails on it. */
    const char *sql =
        "PRAGMA user_version=4;"
        "CREATE TABLE preferences(key TEXT,value TEXT,updated_at TEXT);"
        "INSERT INTO preferences VALUES('provider.ollama.model','legacy-qwen','2026-01-01T00:00:00Z');"
        "CREATE TABLE providers(id TEXT,type TEXT,model TEXT,capabilities INTEGER,capabilities_known INTEGER,enabled INTEGER,updated_at TEXT);"
        "INSERT INTO providers VALUES('ollama','ollama','legacy-qwen',1,1,1,'2026-01-01T00:00:00Z');";
    char *error = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &error);
    if (error) sqlite3_free(error);
    sqlite3_close(db);
    return rc == SQLITE_OK;
}

static int column_is_pk(sqlite3 *db, const char *table, const char *column) {
    char sql[160];
    snprintf(sql, sizeof(sql), "PRAGMA table_info(\"%s\")", table);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    int found = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        if (name && strcmp(name, column) == 0) {
            found = sqlite3_column_int(stmt, 5) > 0;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

int main(void) {
    char path[] = "/tmp/atena-constraint-migration-XXXXXX";
    int fd = mkstemp(path);
    ATENA_TEST_ASSERT(fd >= 0);
    close(fd);
    unlink(path);
    ATENA_TEST_ASSERT(create_broken_constraint_database(path));

    AtenaCoreConfig config = {path, "identity", 12000, 2, 1};
    AtenaCore *core = NULL;
    ATENA_TEST_ASSERT(atena_core_create(&config, &core) == ATENA_OK);
    ATENA_TEST_ASSERT(atena_core_provider_configure(core, "ollama", "ollama",
                                                    "http://127.0.0.1:11434",
                                                    "qwen3:0.6b", NULL) == ATENA_OK);
    ATENA_TEST_ASSERT(atena_core_model_select(core, "ollama", "qwen3:0.6b") == ATENA_OK);
    ATENA_TEST_ASSERT(atena_core_memory_put(core, "test.constraint.repair", "ok") == ATENA_OK);
    atena_core_destroy(core);

    sqlite3 *db = NULL;
    ATENA_TEST_ASSERT(sqlite3_open(path, &db) == SQLITE_OK);
    ATENA_TEST_ASSERT(column_is_pk(db, "preferences", "key"));
    ATENA_TEST_ASSERT(column_is_pk(db, "providers", "id"));

    sqlite3_stmt *stmt = NULL;
    ATENA_TEST_ASSERT(sqlite3_prepare_v2(db,
        "SELECT value FROM preferences WHERE key='provider.ollama.model'",
        -1, &stmt, NULL) == SQLITE_OK);
    ATENA_TEST_ASSERT(sqlite3_step(stmt) == SQLITE_ROW);
    const char *model = (const char *)sqlite3_column_text(stmt, 0);
    ATENA_TEST_ASSERT(model && strcmp(model, "qwen3:0.6b") == 0);
    sqlite3_finalize(stmt);

    ATENA_TEST_ASSERT(sqlite3_prepare_v2(db,
        "SELECT model FROM providers WHERE id='ollama'",
        -1, &stmt, NULL) == SQLITE_OK);
    ATENA_TEST_ASSERT(sqlite3_step(stmt) == SQLITE_ROW);
    model = (const char *)sqlite3_column_text(stmt, 0);
    ATENA_TEST_ASSERT(model && strcmp(model, "qwen3:0.6b") == 0);
    sqlite3_finalize(stmt);

    ATENA_TEST_ASSERT(sqlite3_prepare_v2(db, "PRAGMA user_version", -1, &stmt, NULL) == SQLITE_OK);
    ATENA_TEST_ASSERT(sqlite3_step(stmt) == SQLITE_ROW);
    ATENA_TEST_ASSERT(sqlite3_column_int(stmt, 0) == 5);
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    unlink(path);
    puts("test_migration_constraints: PASS");
    return 0;
}
