#include "store.h"
#include "../core/util.h"
#include <json-c/json.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ATENA_SCHEMA_VERSION 4

static AtenaStatus exec_sql(sqlite3 *db, const char *sql) {
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr,"Atena SQLite: rc=%d (%s), operation=exec_sql%s%s\n",
                rc, sqlite3_errstr(rc), err ? ", detail=" : "", err ? err : "");
        sqlite3_free(err);
        return ATENA_ERR_DB;
    }
    return ATENA_OK;
}

static AtenaStatus table_has_column(sqlite3 *db, const char *table, const char *column, int *out_has) {
    if (!db || !table || !column || !out_has) return ATENA_ERR_INVALID_ARGUMENT;
    *out_has = 0;

    char sql[160];
    int n = snprintf(sql, sizeof(sql), "PRAGMA table_info(\"%s\")", table);
    if (n < 0 || (size_t)n >= sizeof(sql)) return ATENA_ERR_INVALID_ARGUMENT;

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return ATENA_ERR_DB;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        if (name && strcmp(name, column) == 0) {
            *out_has = 1;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return (rc == SQLITE_ROW || rc == SQLITE_DONE) ? ATENA_OK : ATENA_ERR_DB;
}

static AtenaStatus table_exists(sqlite3 *db, const char *table, int *out_exists) {
    if (!db || !table || !out_exists) return ATENA_ERR_INVALID_ARGUMENT;
    *out_exists = 0;
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db,
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=? LIMIT 1",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) return ATENA_ERR_DB;
    sqlite3_bind_text(stmt, 1, table, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) *out_exists = 1;
    sqlite3_finalize(stmt);
    return (rc == SQLITE_ROW || rc == SQLITE_DONE) ? ATENA_OK : ATENA_ERR_DB;
}

static AtenaStatus first_existing_column(sqlite3 *db,
                                         const char *table,
                                         const char *const *candidates,
                                         size_t count,
                                         const char **out_column) {
    if (!db || !table || !candidates || !out_column) return ATENA_ERR_INVALID_ARGUMENT;
    *out_column = NULL;
    for (size_t i = 0; i < count; i++) {
        int has = 0;
        AtenaStatus st = table_has_column(db, table, candidates[i], &has);
        if (st != ATENA_OK) return st;
        if (has) { *out_column = candidates[i]; return ATENA_OK; }
    }
    return ATENA_OK;
}

static AtenaStatus next_backup_table_name(sqlite3 *db,
                                          const char *base,
                                          char out[128]) {
    if (!db || !base || !out) return ATENA_ERR_INVALID_ARGUMENT;
    for (int i = 0; i < 1000; i++) {
        int n = i == 0 ? snprintf(out, 128, "%s_legacy_v4", base)
                       : snprintf(out, 128, "%s_legacy_v4_%d", base, i);
        if (n < 0 || n >= 128) return ATENA_ERR_INVALID_ARGUMENT;
        int exists = 0;
        AtenaStatus st = table_exists(db, out, &exists);
        if (st != ATENA_OK) return st;
        if (!exists) return ATENA_OK;
    }
    return ATENA_ERR_CONFLICT;
}

static AtenaStatus repair_preferences_schema(sqlite3 *db) {
    int exists = 0;
    AtenaStatus st = table_exists(db, "preferences", &exists);
    if (st != ATENA_OK || !exists) return st;

    int has_key = 0, has_value = 0, has_updated = 0;
    if ((st = table_has_column(db, "preferences", "key", &has_key)) != ATENA_OK) return st;
    if ((st = table_has_column(db, "preferences", "value", &has_value)) != ATENA_OK) return st;
    if ((st = table_has_column(db, "preferences", "updated_at", &has_updated)) != ATENA_OK) return st;
    if (has_key && has_value && has_updated) return ATENA_OK;

    /* The common intermediate schema already had `key`, but used another
     * value/timestamp column. Repair it in place so UNIQUE/PK semantics stay. */
    if (has_key) {
        const char *value_alias = NULL;
        const char *time_alias = NULL;
        static const char *value_candidates[] = {"data", "payload", "pref_value", "json", "text"};
        static const char *time_candidates[] = {"created_at", "timestamp", "modified_at"};
        if (!has_value) {
            st = first_existing_column(db, "preferences", value_candidates,
                                       sizeof(value_candidates)/sizeof(value_candidates[0]), &value_alias);
            if (st != ATENA_OK) return st;
            st = exec_sql(db, "ALTER TABLE preferences ADD COLUMN value TEXT NOT NULL DEFAULT '';" );
            if (st != ATENA_OK) return st;
            if (value_alias) {
                char sql[384];
                int n = snprintf(sql, sizeof(sql),
                    "UPDATE preferences SET value=COALESCE(CAST(\"%s\" AS TEXT),'') WHERE value='';",
                    value_alias);
                if (n < 0 || (size_t)n >= sizeof(sql)) return ATENA_ERR_INVALID_ARGUMENT;
                if ((st = exec_sql(db, sql)) != ATENA_OK) return st;
            }
        }
        if (!has_updated) {
            st = first_existing_column(db, "preferences", time_candidates,
                                       sizeof(time_candidates)/sizeof(time_candidates[0]), &time_alias);
            if (st != ATENA_OK) return st;
            st = exec_sql(db, "ALTER TABLE preferences ADD COLUMN updated_at TEXT NOT NULL DEFAULT '';" );
            if (st != ATENA_OK) return st;
            if (time_alias) {
                char sql[384];
                int n = snprintf(sql, sizeof(sql),
                    "UPDATE preferences SET updated_at=COALESCE(CAST(\"%s\" AS TEXT),'') WHERE updated_at='';",
                    time_alias);
                if (n < 0 || (size_t)n >= sizeof(sql)) return ATENA_ERR_INVALID_ARGUMENT;
                if ((st = exec_sql(db, sql)) != ATENA_OK) return st;
            }
            st = exec_sql(db,
                "UPDATE preferences SET updated_at=strftime('%Y-%m-%dT%H:%M:%fZ','now') WHERE updated_at='';");
            if (st != ATENA_OK) return st;
        }
        fprintf(stderr, "Atena SQLite: tabela preferences legada reparada in-place.\n");
        return ATENA_OK;
    }

    /* Unknown/older shape: preserve the whole original table, create the
     * canonical table and copy only fields we can identify safely. */
    char backup[128], sql[1024];
    if ((st = next_backup_table_name(db, "preferences", backup)) != ATENA_OK) return st;
    int n = snprintf(sql, sizeof(sql), "ALTER TABLE preferences RENAME TO \"%s\";", backup);
    if (n < 0 || (size_t)n >= sizeof(sql)) return ATENA_ERR_INVALID_ARGUMENT;
    if ((st = exec_sql(db, sql)) != ATENA_OK) return st;
    if ((st = exec_sql(db,
        "CREATE TABLE preferences(key TEXT PRIMARY KEY,value TEXT NOT NULL,updated_at TEXT NOT NULL);")) != ATENA_OK) return st;

    static const char *key_candidates[] = {"name", "pref_key", "id"};
    static const char *value_candidates[] = {"value", "data", "payload", "pref_value", "json", "text"};
    static const char *time_candidates[] = {"updated_at", "created_at", "timestamp", "modified_at"};
    const char *key_col = NULL, *value_col = NULL, *time_col = NULL;
    if ((st = first_existing_column(db, backup, key_candidates, sizeof(key_candidates)/sizeof(key_candidates[0]), &key_col)) != ATENA_OK) return st;
    if ((st = first_existing_column(db, backup, value_candidates, sizeof(value_candidates)/sizeof(value_candidates[0]), &value_col)) != ATENA_OK) return st;
    if ((st = first_existing_column(db, backup, time_candidates, sizeof(time_candidates)/sizeof(time_candidates[0]), &time_col)) != ATENA_OK) return st;
    if (key_col && value_col) {
        if (time_col) {
            n = snprintf(sql, sizeof(sql),
                "INSERT OR REPLACE INTO preferences(key,value,updated_at) "
                "SELECT CAST(\"%s\" AS TEXT),COALESCE(CAST(\"%s\" AS TEXT),''),"
                "COALESCE(CAST(\"%s\" AS TEXT),strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ','now')) "
                "FROM \"%s\" WHERE \"%s\" IS NOT NULL;",
                key_col, value_col, time_col, backup, key_col);
        } else {
            n = snprintf(sql, sizeof(sql),
                "INSERT OR REPLACE INTO preferences(key,value,updated_at) "
                "SELECT CAST(\"%s\" AS TEXT),COALESCE(CAST(\"%s\" AS TEXT),''),"
                "strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ','now') "
                "FROM \"%s\" WHERE \"%s\" IS NOT NULL;",
                key_col, value_col, backup, key_col);
        }
        if (n < 0 || (size_t)n >= sizeof(sql)) return ATENA_ERR_INVALID_ARGUMENT;
        if ((st = exec_sql(db, sql)) != ATENA_OK) return st;
    }
    fprintf(stderr, "Atena SQLite: preferences incompatível preservada como %s e esquema canônico criado.\n", backup);
    return ATENA_OK;
}

static AtenaStatus repair_providers_schema(sqlite3 *db) {
    int exists = 0, has_id = 0;
    AtenaStatus st = table_exists(db, "providers", &exists);
    if (st != ATENA_OK || !exists) return st;
    if ((st = table_has_column(db, "providers", "id", &has_id)) != ATENA_OK) return st;

    if (!has_id) {
        char backup[128], sql[384];
        if ((st = next_backup_table_name(db, "providers", backup)) != ATENA_OK) return st;
        int n = snprintf(sql, sizeof(sql), "ALTER TABLE providers RENAME TO \"%s\";", backup);
        if (n < 0 || (size_t)n >= sizeof(sql)) return ATENA_ERR_INVALID_ARGUMENT;
        if ((st = exec_sql(db, sql)) != ATENA_OK) return st;
        st = exec_sql(db,
            "CREATE TABLE providers("
            "id TEXT PRIMARY KEY,type TEXT NOT NULL,model TEXT NOT NULL,capabilities INTEGER NOT NULL,"
            "capabilities_known INTEGER NOT NULL DEFAULT 0,enabled INTEGER NOT NULL,updated_at TEXT NOT NULL);" );
        if (st == ATENA_OK)
            fprintf(stderr, "Atena SQLite: providers incompatível preservada como %s e esquema canônico criado.\n", backup);
        return st;
    }

    struct Repair { const char *column; const char *sql; } repairs[] = {
        {"type", "ALTER TABLE providers ADD COLUMN type TEXT NOT NULL DEFAULT 'openai_compatible';"},
        {"model", "ALTER TABLE providers ADD COLUMN model TEXT NOT NULL DEFAULT '';"},
        {"capabilities", "ALTER TABLE providers ADD COLUMN capabilities INTEGER NOT NULL DEFAULT 0;"},
        {"capabilities_known", "ALTER TABLE providers ADD COLUMN capabilities_known INTEGER NOT NULL DEFAULT 0;"},
        {"enabled", "ALTER TABLE providers ADD COLUMN enabled INTEGER NOT NULL DEFAULT 1;"},
        {"updated_at", "ALTER TABLE providers ADD COLUMN updated_at TEXT NOT NULL DEFAULT '';"},
    };
    int changed = 0;
    for (size_t i = 0; i < sizeof(repairs)/sizeof(repairs[0]); i++) {
        int has = 0;
        if ((st = table_has_column(db, "providers", repairs[i].column, &has)) != ATENA_OK) return st;
        if (!has) {
            if ((st = exec_sql(db, repairs[i].sql)) != ATENA_OK) return st;
            changed = 1;
        }
    }
    if (changed) {
        if ((st = exec_sql(db,
            "UPDATE providers SET updated_at=strftime('%Y-%m-%dT%H:%M:%fZ','now') WHERE updated_at='';")) != ATENA_OK) return st;
        fprintf(stderr, "Atena SQLite: tabela providers legada reparada in-place.\n");
    }
    return ATENA_OK;
}

static AtenaStatus migrate_legacy_column(sqlite3 *db,
                                         const char *table,
                                         const char *legacy_column,
                                         const char *current_column) {
    int has_current = 0;
    int has_legacy = 0;
    AtenaStatus st = table_has_column(db, table, current_column, &has_current);
    if (st != ATENA_OK) return st;
    if (has_current) return ATENA_OK;

    st = table_has_column(db, table, legacy_column, &has_legacy);
    if (st != ATENA_OK) return st;
    if (!has_legacy) return ATENA_ERR_NOT_FOUND;

    char sql[320];
    int n = snprintf(sql, sizeof(sql),
                     "ALTER TABLE \"%s\" RENAME COLUMN \"%s\" TO \"%s\";",
                     table, legacy_column, current_column);
    if (n < 0 || (size_t)n >= sizeof(sql)) return ATENA_ERR_INVALID_ARGUMENT;
    return exec_sql(db, sql);
}

static AtenaStatus ensure_column(sqlite3 *db,
                                 const char *table,
                                 const char *column,
                                 const char *alter_sql) {
    int has_column = 0;
    AtenaStatus st = table_has_column(db, table, column, &has_column);
    if (st != ATENA_OK) return st;
    if (has_column) return ATENA_OK;
    return exec_sql(db, alter_sql);
}

static AtenaStatus verify_column(sqlite3 *db, const char *table, const char *column) {
    int has_column = 0;
    AtenaStatus st = table_has_column(db, table, column, &has_column);
    if (st != ATENA_OK) return st;
    if (!has_column) {
        fprintf(stderr, "Atena SQLite: schema incompatível: tabela '%s' sem coluna '%s'\n",
                table, column);
        return ATENA_ERR_DB;
    }
    return ATENA_OK;
}

static AtenaStatus migrate(AtenaStore *store) {
    sqlite3_stmt *stmt = NULL;
    int version = 0;
    if (sqlite3_prepare_v2(store->db, "PRAGMA user_version", -1, &stmt, NULL) != SQLITE_OK)
        return ATENA_ERR_DB;
    if (sqlite3_step(stmt) == SQLITE_ROW) version = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (version > ATENA_SCHEMA_VERSION) return ATENA_ERR_SCHEMA_TOO_NEW;

    AtenaStatus status = exec_sql(store->db, "PRAGMA foreign_keys=ON;");
    if (status != ATENA_OK) return status;

    /* journal_mode cannot be changed inside a transaction. */
    status = exec_sql(store->db, "PRAGMA journal_mode=WAL;");
    if (status != ATENA_OK) return status;

    status = exec_sql(store->db, "BEGIN IMMEDIATE;");
    if (status != ATENA_OK) return status;

    /*
     * IMPORTANT: indexes that reference migrated columns are deliberately NOT
     * created here. CREATE TABLE IF NOT EXISTS does not repair an old table.
     * An older database may still have messages.conversation_id, so creating
     * idx_messages_session_created before migration would fail with
     * "no such column: session_id" and prevent the Core from starting.
     */
    const char *base_schema =
        "CREATE TABLE IF NOT EXISTS sessions("
        " id TEXT PRIMARY KEY, title TEXT NOT NULL DEFAULT '', created_at TEXT NOT NULL, updated_at TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS messages("
        " id TEXT PRIMARY KEY, session_id TEXT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,"
        " role INTEGER NOT NULL, state INTEGER NOT NULL, content TEXT NOT NULL, created_at TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS preferences("
        " key TEXT PRIMARY KEY, value TEXT NOT NULL, updated_at TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS documents("
        " id TEXT PRIMARY KEY, title TEXT NOT NULL, locator TEXT NOT NULL, content_hash TEXT NOT NULL UNIQUE, created_at TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS chunks("
        " id TEXT PRIMARY KEY, document_id TEXT NOT NULL REFERENCES documents(id) ON DELETE CASCADE,"
        " ordinal INTEGER NOT NULL, locator TEXT NOT NULL, content TEXT NOT NULL);"
        "CREATE VIRTUAL TABLE IF NOT EXISTS chunks_fts USING fts5(chunk_id UNINDEXED, content, tokenize='unicode61');"
        "CREATE TABLE IF NOT EXISTS operations("
        " id TEXT PRIMARY KEY, session_id TEXT REFERENCES sessions(id) ON DELETE SET NULL, idempotency_key TEXT,"
        " state TEXT NOT NULL, status_code INTEGER NOT NULL DEFAULT 0, created_at TEXT NOT NULL, updated_at TEXT NOT NULL,"
        " UNIQUE(session_id, idempotency_key));"
        "CREATE TABLE IF NOT EXISTS providers("
        " id TEXT PRIMARY KEY, type TEXT NOT NULL, model TEXT NOT NULL, capabilities INTEGER NOT NULL,"
        " capabilities_known INTEGER NOT NULL DEFAULT 0, enabled INTEGER NOT NULL, updated_at TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS audit_events("
        " id INTEGER PRIMARY KEY AUTOINCREMENT, event TEXT NOT NULL, session_id TEXT, operation_id TEXT,"
        " details TEXT NOT NULL DEFAULT '', created_at TEXT NOT NULL);";

    status = exec_sql(store->db, base_schema);
    if (status != ATENA_OK) goto rollback;

    /* Intermediate releases shipped tables whose names matched the current
     * schema while their columns did not. CREATE TABLE IF NOT EXISTS cannot
     * repair that situation, so normalize those tables explicitly. */
    status = repair_preferences_schema(store->db);
    if (status != ATENA_OK) goto rollback;
    status = repair_providers_schema(store->db);
    if (status != ATENA_OK) goto rollback;

    /* v0/v1 compatibility: conversation_id was the old public name. */
    status = migrate_legacy_column(store->db, "messages", "conversation_id", "session_id");
    if (status != ATENA_OK && status != ATENA_ERR_NOT_FOUND) goto rollback;

    status = migrate_legacy_column(store->db, "operations", "conversation_id", "session_id");
    if (status != ATENA_OK && status != ATENA_ERR_NOT_FOUND) goto rollback;

    status = migrate_legacy_column(store->db, "audit_events", "conversation_id", "session_id");
    if (status != ATENA_OK && status != ATENA_ERR_NOT_FOUND) goto rollback;

    /* Additive repairs for databases produced by intermediate builds. */
    status = ensure_column(store->db, "messages", "state",
                           "ALTER TABLE messages ADD COLUMN state INTEGER NOT NULL DEFAULT 0;");
    if (status != ATENA_OK) goto rollback;

    status = ensure_column(store->db, "providers", "capabilities_known",
                           "ALTER TABLE providers ADD COLUMN capabilities_known INTEGER NOT NULL DEFAULT 0;");
    if (status != ATENA_OK) goto rollback;

    status = ensure_column(store->db, "operations", "status_code",
                           "ALTER TABLE operations ADD COLUMN status_code INTEGER NOT NULL DEFAULT 0;");
    if (status != ATENA_OK) goto rollback;

    status = ensure_column(store->db, "audit_events", "details",
                           "ALTER TABLE audit_events ADD COLUMN details TEXT NOT NULL DEFAULT '';");
    if (status != ATENA_OK) goto rollback;

    /* Fail early with a useful diagnostic if a legacy table is too different. */
    status = verify_column(store->db, "messages", "session_id");
    if (status != ATENA_OK) goto rollback;
    status = verify_column(store->db, "messages", "id");
    if (status != ATENA_OK) goto rollback;
    status = verify_column(store->db, "messages", "role");
    if (status != ATENA_OK) goto rollback;
    status = verify_column(store->db, "messages", "content");
    if (status != ATENA_OK) goto rollback;
    status = verify_column(store->db, "messages", "created_at");
    if (status != ATENA_OK) goto rollback;

    /* Convert textual role/state values from very old prototypes when present. */
    status = exec_sql(store->db,
        "UPDATE messages SET role=CASE lower(CAST(role AS TEXT)) "
        " WHEN 'system' THEN 0 WHEN 'user' THEN 1 WHEN 'assistant' THEN 2 WHEN 'tool' THEN 3 "
        " ELSE CAST(role AS INTEGER) END WHERE typeof(role)='text';");
    if (status != ATENA_OK) goto rollback;

    status = exec_sql(store->db,
        "UPDATE messages SET state=CASE lower(CAST(state AS TEXT)) "
        " WHEN 'complete' THEN 0 WHEN 'partial' THEN 1 WHEN 'cancelled' THEN 2 "
        " WHEN 'interrupted' THEN 3 WHEN 'failed' THEN 4 "
        " ELSE CAST(state AS INTEGER) END WHERE typeof(state)='text';");
    if (status != ATENA_OK) goto rollback;

    /* Ensure every migrated message points to a session row. */
    status = exec_sql(store->db,
        "INSERT OR IGNORE INTO sessions(id,title,created_at,updated_at) "
        "SELECT session_id,'',MIN(created_at),MAX(created_at) FROM messages "
        "WHERE session_id IS NOT NULL AND session_id<>'' GROUP BY session_id;");
    if (status != ATENA_OK) goto rollback;

    /* Indexes are safe only after legacy columns have been repaired. */
    status = exec_sql(store->db,
        "CREATE INDEX IF NOT EXISTS idx_messages_session_created "
        "ON messages(session_id, created_at);");
    if (status != ATENA_OK) goto rollback;

    status = exec_sql(store->db, "PRAGMA user_version=4;");
    if (status != ATENA_OK) goto rollback;

    status = exec_sql(store->db, "COMMIT;");
    return status;

rollback:
    (void)exec_sql(store->db, "ROLLBACK;");
    return status;
}

AtenaStatus atena_store_open(const char *path, AtenaStore **out_store) {
    if (!path || !out_store) return ATENA_ERR_INVALID_ARGUMENT;
    *out_store = NULL;
    AtenaStore *s = calloc(1, sizeof(*s));
    if (!s) return ATENA_ERR_NO_MEMORY;
    int open_rc=sqlite3_open_v2(path, &s->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
    if (open_rc != SQLITE_OK) {
        fprintf(stderr,"Atena SQLite: path=%s, rc=%d (%s), operation=open\n",path,open_rc,sqlite3_errstr(open_rc));
        if (s->db) sqlite3_close(s->db);
        free(s);
        return ATENA_ERR_DB;
    }
    sqlite3_busy_timeout(s->db, 3000);
    AtenaStatus st = migrate(s);
    if (st != ATENA_OK) {
        sqlite3_close(s->db);
        free(s);
        return st;
    }
    *out_store = s;
    return ATENA_OK;
}

void atena_store_close(AtenaStore *store) {
    if (!store) return;
    sqlite3_close(store->db);
    free(store);
}

AtenaStatus atena_store_session_create(AtenaStore *store, const char *title, char out_id[37]) {
    if (!store || !out_id) return ATENA_ERR_INVALID_ARGUMENT;
    if (!atena_uuid4(out_id)) return ATENA_ERR_IO;
    char now[32]; atena_now_iso8601(now);
    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT INTO sessions(id,title,created_at,updated_at) VALUES(?,?,?,?)";
    if (sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL) != SQLITE_OK) return ATENA_ERR_DB;
    sqlite3_bind_text(stmt, 1, out_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, title ? title : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, now, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, now, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? ATENA_OK : ATENA_ERR_DB;
}

AtenaStatus atena_store_session_exists(AtenaStore *store, const char *session_id, int *out_exists) {
    if (!store || !session_id || !out_exists) return ATENA_ERR_INVALID_ARGUMENT;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(store->db, "SELECT 1 FROM sessions WHERE id=?", -1, &stmt, NULL) != SQLITE_OK) return ATENA_ERR_DB;
    sqlite3_bind_text(stmt, 1, session_id, -1, SQLITE_TRANSIENT);
    *out_exists = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return ATENA_OK;
}

AtenaStatus atena_store_message_add(AtenaStore *store, const AtenaMessage *message) {
    if (!store || !message || !message->id[0] || !message->session_id[0] || !message->content) return ATENA_ERR_INVALID_ARGUMENT;
    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT INTO messages(id,session_id,role,state,content,created_at) VALUES(?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL) != SQLITE_OK) return ATENA_ERR_DB;
    sqlite3_bind_text(stmt, 1, message->id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, message->session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, (int)message->role);
    sqlite3_bind_int(stmt, 4, (int)message->state);
    sqlite3_bind_text(stmt, 5, message->content, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, message->created_at, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? ATENA_OK : (rc == SQLITE_CONSTRAINT ? ATENA_ERR_CONFLICT : ATENA_ERR_DB);
}

AtenaStatus atena_store_message_state(AtenaStore *store, const char *message_id, AtenaMessageState state) {
    if (!store || !message_id) return ATENA_ERR_INVALID_ARGUMENT;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(store->db, "UPDATE messages SET state=? WHERE id=?", -1, &stmt, NULL) != SQLITE_OK) return ATENA_ERR_DB;
    sqlite3_bind_int(stmt, 1, (int)state);
    sqlite3_bind_text(stmt, 2, message_id, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? ATENA_OK : ATENA_ERR_DB;
}

AtenaStatus atena_store_history(AtenaStore *store, const char *session_id, AtenaMessage **out_messages, size_t *out_count) {
    if (!store || !session_id || !out_messages || !out_count) return ATENA_ERR_INVALID_ARGUMENT;
    *out_messages = NULL; *out_count = 0;
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT id,role,state,content,created_at FROM messages WHERE session_id=? ORDER BY rowid ASC";
    if (sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL) != SQLITE_OK) return ATENA_ERR_DB;
    sqlite3_bind_text(stmt, 1, session_id, -1, SQLITE_TRANSIENT);
    size_t cap = 0, count = 0;
    AtenaMessage *items = NULL;
    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (count == cap) {
            size_t next = cap ? cap * 2 : 8;
            AtenaMessage *tmp = realloc(items, next * sizeof(*tmp));
            if (!tmp) { sqlite3_finalize(stmt); goto oom; }
            items = tmp; cap = next;
        }
        AtenaMessage *m = &items[count]; memset(m, 0, sizeof(*m));
        snprintf(m->id, sizeof(m->id), "%s", sqlite3_column_text(stmt, 0));
        snprintf(m->session_id, sizeof(m->session_id), "%s", session_id);
        m->role = (AtenaRole)sqlite3_column_int(stmt, 1);
        m->state = (AtenaMessageState)sqlite3_column_int(stmt, 2);
        const char *content = (const char *)sqlite3_column_text(stmt, 3);
        m->content = atena_strdup(content ? content : "");
        if (!m->content) { sqlite3_finalize(stmt); goto oom; }
        snprintf(m->created_at, sizeof(m->created_at), "%s", sqlite3_column_text(stmt, 4));
        count++;
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) { for (size_t i=0;i<count;i++) free(items[i].content); free(items); return ATENA_ERR_DB; }
    *out_messages = items; *out_count = count; return ATENA_OK;
oom:
    for (size_t i=0;i<count;i++) free(items[i].content);
    free(items);
    return ATENA_ERR_NO_MEMORY;
}

AtenaStatus atena_store_preference_put(AtenaStore *store, const char *key, const char *value) {
    if (!store || !key || !*key || !value) return ATENA_ERR_INVALID_ARGUMENT;
    char now[32]; atena_now_iso8601(now);
    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT INTO preferences(key,value,updated_at) VALUES(?,?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value,updated_at=excluded.updated_at";
    if (sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL) != SQLITE_OK) return ATENA_ERR_DB;
    sqlite3_bind_text(stmt,1,key,-1,SQLITE_TRANSIENT); sqlite3_bind_text(stmt,2,value,-1,SQLITE_TRANSIENT); sqlite3_bind_text(stmt,3,now,-1,SQLITE_TRANSIENT);
    int rc=sqlite3_step(stmt); sqlite3_finalize(stmt); return rc==SQLITE_DONE?ATENA_OK:ATENA_ERR_DB;
}

AtenaStatus atena_store_preference_get(AtenaStore *store, const char *key, char **out_value) {
    if (!store || !key || !*key || !out_value) return ATENA_ERR_INVALID_ARGUMENT;
    *out_value = NULL;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(store->db, "SELECT value FROM preferences WHERE key=?", -1, &stmt, NULL) != SQLITE_OK)
        return ATENA_ERR_DB;
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const unsigned char *raw = sqlite3_column_text(stmt, 0);
        *out_value = atena_strdup(raw ? (const char *)raw : "");
        sqlite3_finalize(stmt);
        return *out_value ? ATENA_OK : ATENA_ERR_NO_MEMORY;
    }
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? ATENA_ERR_NOT_FOUND : ATENA_ERR_DB;
}

AtenaStatus atena_store_preferences_text(AtenaStore *store, char **out_text) {
    if (!store || !out_text) return ATENA_ERR_INVALID_ARGUMENT;
    *out_text = NULL;
    sqlite3_stmt *stmt=NULL;
    if (sqlite3_prepare_v2(store->db,"SELECT key,value FROM preferences ORDER BY key",-1,&stmt,NULL)!=SQLITE_OK) return ATENA_ERR_DB;
    size_t cap=256,len=0; char *buf=malloc(cap); if(!buf){sqlite3_finalize(stmt);return ATENA_ERR_NO_MEMORY;} buf[0]='\0';
    int rc;
    while((rc=sqlite3_step(stmt))==SQLITE_ROW){
        const char *k=(const char*)sqlite3_column_text(stmt,0); const char *v=(const char*)sqlite3_column_text(stmt,1);
        size_t need=strlen(k)+strlen(v)+5;
        if(len+need+1>cap){while(len+need+1>cap)cap*=2; char *t=realloc(buf,cap); if(!t){free(buf);sqlite3_finalize(stmt);return ATENA_ERR_NO_MEMORY;} buf=t;}
        len += (size_t)snprintf(buf+len,cap-len,"- %s: %s\n",k,v);
    }
    sqlite3_finalize(stmt); if(rc!=SQLITE_DONE){free(buf);return ATENA_ERR_DB;} *out_text=buf; return ATENA_OK;
}

static void content_sha256(const char *text, char out[72]) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char*)text, strlen(text), digest);
    memcpy(out,"sha256:",7);
    for(size_t i=0;i<SHA256_DIGEST_LENGTH;i++)snprintf(out+7+i*2,3,"%02x",digest[i]);
}

AtenaStatus atena_store_rag_import_text(AtenaStore *store, const char *title, const char *locator, const char *text, char out_document_id[37]) {
    if (!store || !title || !locator || !text || !out_document_id) return ATENA_ERR_INVALID_ARGUMENT;
    char hash[72]; content_sha256(text,hash);
    sqlite3_stmt *check=NULL;
    if(sqlite3_prepare_v2(store->db,"SELECT id FROM documents WHERE content_hash=?",-1,&check,NULL)!=SQLITE_OK)return ATENA_ERR_DB;
    sqlite3_bind_text(check,1,hash,-1,SQLITE_TRANSIENT);
    if(sqlite3_step(check)==SQLITE_ROW){snprintf(out_document_id,37,"%s",sqlite3_column_text(check,0));sqlite3_finalize(check);return ATENA_OK;}
    sqlite3_finalize(check);
    if(!atena_uuid4(out_document_id)) return ATENA_ERR_IO;
    char now[32]; atena_now_iso8601(now);
    if(exec_sql(store->db,"BEGIN IMMEDIATE")!=ATENA_OK)return ATENA_ERR_DB;
    sqlite3_stmt *doc=NULL;
    if(sqlite3_prepare_v2(store->db,"INSERT INTO documents(id,title,locator,content_hash,created_at) VALUES(?,?,?,?,?)",-1,&doc,NULL)!=SQLITE_OK){exec_sql(store->db,"ROLLBACK");return ATENA_ERR_DB;}
    sqlite3_bind_text(doc,1,out_document_id,-1,SQLITE_TRANSIENT);sqlite3_bind_text(doc,2,title,-1,SQLITE_TRANSIENT);sqlite3_bind_text(doc,3,locator,-1,SQLITE_TRANSIENT);sqlite3_bind_text(doc,4,hash,-1,SQLITE_TRANSIENT);sqlite3_bind_text(doc,5,now,-1,SQLITE_TRANSIENT);
    if(sqlite3_step(doc)!=SQLITE_DONE){sqlite3_finalize(doc);exec_sql(store->db,"ROLLBACK");return ATENA_ERR_DB;} sqlite3_finalize(doc);
    const size_t chunk_chars=1200, overlap=120;
    size_t text_len=strlen(text), start=0; int ordinal=0;
    while(start<text_len){
        size_t end=start+chunk_chars; if(end>text_len)end=text_len;
        if(end<text_len){ while(end>start+600 && text[end]!='\n' && text[end]!='.') end--; if(end<=start+600) end=start+chunk_chars; }
        char *content=atena_strndup(text+start,end-start); if(!content){exec_sql(store->db,"ROLLBACK");return ATENA_ERR_NO_MEMORY;}
        char chunk_id[37]; if(!atena_uuid4(chunk_id)){free(content);exec_sql(store->db,"ROLLBACK");return ATENA_ERR_IO;}
        char chunk_loc[512]; snprintf(chunk_loc,sizeof(chunk_loc),"%s#chars=%zu-%zu",locator,start,end);
        sqlite3_stmt *cs=NULL;
        if(sqlite3_prepare_v2(store->db,"INSERT INTO chunks(id,document_id,ordinal,locator,content) VALUES(?,?,?,?,?)",-1,&cs,NULL)!=SQLITE_OK){free(content);exec_sql(store->db,"ROLLBACK");return ATENA_ERR_DB;}
        sqlite3_bind_text(cs,1,chunk_id,-1,SQLITE_TRANSIENT);sqlite3_bind_text(cs,2,out_document_id,-1,SQLITE_TRANSIENT);sqlite3_bind_int(cs,3,ordinal++);sqlite3_bind_text(cs,4,chunk_loc,-1,SQLITE_TRANSIENT);sqlite3_bind_text(cs,5,content,-1,SQLITE_TRANSIENT);
        int crc=sqlite3_step(cs);sqlite3_finalize(cs);if(crc!=SQLITE_DONE){free(content);exec_sql(store->db,"ROLLBACK");return ATENA_ERR_DB;}
        sqlite3_stmt *fs=NULL;
        if(sqlite3_prepare_v2(store->db,"INSERT INTO chunks_fts(chunk_id,content) VALUES(?,?)",-1,&fs,NULL)!=SQLITE_OK){free(content);exec_sql(store->db,"ROLLBACK");return ATENA_ERR_DB;}
        sqlite3_bind_text(fs,1,chunk_id,-1,SQLITE_TRANSIENT);sqlite3_bind_text(fs,2,content,-1,SQLITE_TRANSIENT);int frc=sqlite3_step(fs);sqlite3_finalize(fs);free(content);if(frc!=SQLITE_DONE){exec_sql(store->db,"ROLLBACK");return ATENA_ERR_DB;}
        if(end==text_len) break;
        start = end > overlap ? end - overlap : end;
    }
    if(exec_sql(store->db,"COMMIT")!=ATENA_OK){exec_sql(store->db,"ROLLBACK");return ATENA_ERR_DB;}
    return ATENA_OK;
}

static char *fts_query(const char *query) {
    size_t n=strlen(query), cap=n*3+8, len=0; char *out=malloc(cap); if(!out)return NULL; out[0]='\0';
    const char *p=query; int terms=0;
    while(*p && terms<12){ while(*p && !((*p>='0'&&*p<='9')||(*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')||(unsigned char)*p>=0x80))p++; if(!*p)break; const char *s=p; while(*p && ((*p>='0'&&*p<='9')||(*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')||(unsigned char)*p>=0x80))p++; size_t w=(size_t)(p-s); if(w<2)continue; if(len+w+6>cap){cap*=2;char *t=realloc(out,cap);if(!t){free(out);return NULL;}out=t;} if(terms){ memcpy(out+len," OR ",4); len+=4; } out[len++]='"'; memcpy(out+len,s,w);len+=w;out[len++]='"'; out[len]='\0'; terms++; }
    if(!terms){free(out);return atena_strdup("\"\"");} return out;
}

AtenaStatus atena_store_documents_json(AtenaStore *store, char **out_json) {
    if (!store || !out_json) return ATENA_ERR_INVALID_ARGUMENT;
    *out_json = NULL;
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT d.id,d.title,d.locator,d.created_at,COUNT(c.id) "
        "FROM documents d LEFT JOIN chunks c ON c.document_id=d.id "
        "GROUP BY d.id,d.title,d.locator,d.created_at ORDER BY d.created_at DESC";
    if (sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL) != SQLITE_OK) return ATENA_ERR_DB;

    json_object *array = json_object_new_array();
    if (!array) { sqlite3_finalize(stmt); return ATENA_ERR_NO_MEMORY; }
    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        json_object *item = json_object_new_object();
        if (!item) { sqlite3_finalize(stmt); json_object_put(array); return ATENA_ERR_NO_MEMORY; }
        const char *id = (const char *)sqlite3_column_text(stmt, 0);
        const char *title = (const char *)sqlite3_column_text(stmt, 1);
        const char *locator = (const char *)sqlite3_column_text(stmt, 2);
        const char *created = (const char *)sqlite3_column_text(stmt, 3);
        json_object_object_add(item, "id", json_object_new_string(id ? id : ""));
        json_object_object_add(item, "title", json_object_new_string(title ? title : ""));
        json_object_object_add(item, "locator", json_object_new_string(locator ? locator : ""));
        json_object_object_add(item, "created_at", json_object_new_string(created ? created : ""));
        json_object_object_add(item, "chunks", json_object_new_int(sqlite3_column_int(stmt, 4)));
        json_object_array_add(array, item);
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) { json_object_put(array); return ATENA_ERR_DB; }
    *out_json = atena_strdup(json_object_to_json_string_ext(array, JSON_C_TO_STRING_PLAIN));
    json_object_put(array);
    return *out_json ? ATENA_OK : ATENA_ERR_NO_MEMORY;
}

AtenaStatus atena_store_rag_search(AtenaStore *store, const char *query, size_t limit, AtenaRagHit **out_hits, size_t *out_count) {
    if(!store||!query||!out_hits||!out_count||limit==0) return ATENA_ERR_INVALID_ARGUMENT;
    *out_hits=NULL;
    *out_count=0;
    char *match=fts_query(query); if(!match)return ATENA_ERR_NO_MEMORY;
    const char *sql="SELECT c.id,c.document_id,d.title,c.locator,c.content,d.content_hash,bm25(chunks_fts) FROM chunks_fts JOIN chunks c ON c.id=chunks_fts.chunk_id JOIN documents d ON d.id=c.document_id WHERE chunks_fts MATCH ? ORDER BY bm25(chunks_fts) LIMIT ?";
    sqlite3_stmt *stmt=NULL; if(sqlite3_prepare_v2(store->db,sql,-1,&stmt,NULL)!=SQLITE_OK){free(match);return ATENA_ERR_DB;} sqlite3_bind_text(stmt,1,match,-1,SQLITE_TRANSIENT);sqlite3_bind_int(stmt,2,(int)limit);free(match);
    AtenaRagHit *hits=calloc(limit,sizeof(*hits)); if(!hits){sqlite3_finalize(stmt);return ATENA_ERR_NO_MEMORY;} size_t count=0; int rc;
    while((rc=sqlite3_step(stmt))==SQLITE_ROW && count<limit){AtenaRagHit *h=&hits[count++];snprintf(h->chunk_id,37,"%s",sqlite3_column_text(stmt,0));snprintf(h->document_id,37,"%s",sqlite3_column_text(stmt,1));h->title=atena_strdup((const char*)sqlite3_column_text(stmt,2));h->locator=atena_strdup((const char*)sqlite3_column_text(stmt,3));h->content=atena_strdup((const char*)sqlite3_column_text(stmt,4));snprintf(h->content_sha256,sizeof(h->content_sha256),"%s",sqlite3_column_text(stmt,5));h->score=-sqlite3_column_double(stmt,6);if(!h->title||!h->locator||!h->content){sqlite3_finalize(stmt);atena_store_rag_hits_free(hits,count);return ATENA_ERR_NO_MEMORY;}}
    sqlite3_finalize(stmt);if(rc!=SQLITE_DONE){atena_store_rag_hits_free(hits,count);return ATENA_ERR_DB;}*out_hits=hits;*out_count=count;return ATENA_OK;
}

void atena_store_rag_hits_free(AtenaRagHit *hits,size_t count){if(!hits)return;for(size_t i=0;i<count;i++){free(hits[i].title);free(hits[i].locator);free(hits[i].content);}free(hits);}

AtenaStatus atena_store_operation_begin(AtenaStore *store,const char *operation_id,const char *session_id,const char *idempotency_key){if(!store||!operation_id||!session_id)return ATENA_ERR_INVALID_ARGUMENT;char now[32];atena_now_iso8601(now);sqlite3_stmt *s=NULL;const char *sql="INSERT INTO operations(id,session_id,idempotency_key,state,status_code,created_at,updated_at) VALUES(?,?,?,'running',0,?,?)";if(sqlite3_prepare_v2(store->db,sql,-1,&s,NULL)!=SQLITE_OK)return ATENA_ERR_DB;sqlite3_bind_text(s,1,operation_id,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,2,session_id,-1,SQLITE_TRANSIENT);if(idempotency_key&&*idempotency_key)sqlite3_bind_text(s,3,idempotency_key,-1,SQLITE_TRANSIENT);else sqlite3_bind_null(s,3);sqlite3_bind_text(s,4,now,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,5,now,-1,SQLITE_TRANSIENT);int rc=sqlite3_step(s);sqlite3_finalize(s);return rc==SQLITE_DONE?ATENA_OK:(rc==SQLITE_CONSTRAINT?ATENA_ERR_CONFLICT:ATENA_ERR_DB);}
AtenaStatus atena_store_operation_finish(AtenaStore *store,const char *operation_id,const char *state,int status_code){if(!store||!operation_id||!state)return ATENA_ERR_INVALID_ARGUMENT;char now[32];atena_now_iso8601(now);sqlite3_stmt *s=NULL;if(sqlite3_prepare_v2(store->db,"UPDATE operations SET state=?,status_code=?,updated_at=? WHERE id=?",-1,&s,NULL)!=SQLITE_OK)return ATENA_ERR_DB;sqlite3_bind_text(s,1,state,-1,SQLITE_TRANSIENT);sqlite3_bind_int(s,2,status_code);sqlite3_bind_text(s,3,now,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,4,operation_id,-1,SQLITE_TRANSIENT);int rc=sqlite3_step(s);sqlite3_finalize(s);return rc==SQLITE_DONE?ATENA_OK:ATENA_ERR_DB;}
AtenaStatus atena_store_operation_by_key(AtenaStore *store,const char *session_id,const char *key,char out_id[37],char out_state[24]){if(!store||!session_id||!key||!*key||!out_id||!out_state)return ATENA_ERR_INVALID_ARGUMENT;sqlite3_stmt *s=NULL;if(sqlite3_prepare_v2(store->db,"SELECT id,state FROM operations WHERE session_id=? AND idempotency_key=?",-1,&s,NULL)!=SQLITE_OK)return ATENA_ERR_DB;sqlite3_bind_text(s,1,session_id,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,2,key,-1,SQLITE_TRANSIENT);int rc=sqlite3_step(s);if(rc==SQLITE_ROW){snprintf(out_id,37,"%s",sqlite3_column_text(s,0));snprintf(out_state,24,"%s",sqlite3_column_text(s,1));sqlite3_finalize(s);return ATENA_OK;}sqlite3_finalize(s);return rc==SQLITE_DONE?ATENA_ERR_NOT_FOUND:ATENA_ERR_DB;}

AtenaStatus atena_store_provider_upsert(AtenaStore *store,const char *id,const char *type,const char *model,unsigned long long caps,unsigned long long known,int enabled){if(!store||!id||!type||!model)return ATENA_ERR_INVALID_ARGUMENT;char now[32];atena_now_iso8601(now);sqlite3_stmt*s=NULL;const char*sql="INSERT INTO providers(id,type,model,capabilities,capabilities_known,enabled,updated_at) VALUES(?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET type=excluded.type,model=excluded.model,capabilities=excluded.capabilities,capabilities_known=excluded.capabilities_known,enabled=excluded.enabled,updated_at=excluded.updated_at";if(sqlite3_prepare_v2(store->db,sql,-1,&s,NULL)!=SQLITE_OK)return ATENA_ERR_DB;sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,2,type,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,3,model,-1,SQLITE_TRANSIENT);sqlite3_bind_int64(s,4,(sqlite3_int64)caps);sqlite3_bind_int64(s,5,(sqlite3_int64)known);sqlite3_bind_int(s,6,enabled);sqlite3_bind_text(s,7,now,-1,SQLITE_TRANSIENT);int rc=sqlite3_step(s);sqlite3_finalize(s);return rc==SQLITE_DONE?ATENA_OK:ATENA_ERR_DB;}
AtenaStatus atena_store_provider_list_json(AtenaStore*store,char**out_json){if(!store||!out_json)return ATENA_ERR_INVALID_ARGUMENT;*out_json=NULL;json_object*arr=json_object_new_array();if(!arr)return ATENA_ERR_NO_MEMORY;sqlite3_stmt*s=NULL;if(sqlite3_prepare_v2(store->db,"SELECT id,type,model,capabilities,capabilities_known,enabled FROM providers ORDER BY id",-1,&s,NULL)!=SQLITE_OK){json_object_put(arr);return ATENA_ERR_DB;}int rc;while((rc=sqlite3_step(s))==SQLITE_ROW){json_object*o=json_object_new_object();json_object_object_add(o,"id",json_object_new_string((const char*)sqlite3_column_text(s,0)));json_object_object_add(o,"type",json_object_new_string((const char*)sqlite3_column_text(s,1)));json_object_object_add(o,"model",json_object_new_string((const char*)sqlite3_column_text(s,2)));json_object_object_add(o,"capabilities_supported",json_object_new_int64(sqlite3_column_int64(s,3)));json_object_object_add(o,"capabilities_known",json_object_new_int64(sqlite3_column_int64(s,4)));json_object_object_add(o,"enabled",json_object_new_boolean(sqlite3_column_int(s,5)));json_object_object_add(o,"configured",json_object_new_boolean(sqlite3_column_int(s,5)));json_object_array_add(arr,o);}sqlite3_finalize(s);if(rc!=SQLITE_DONE){json_object_put(arr);return ATENA_ERR_DB;}*out_json=atena_strdup(json_object_to_json_string_ext(arr,JSON_C_TO_STRING_PLAIN));json_object_put(arr);return *out_json?ATENA_OK:ATENA_ERR_NO_MEMORY;}
AtenaStatus atena_store_audit(AtenaStore*store,const char*event,const char*session_id,const char*operation_id,const char*details){if(!store||!event)return ATENA_ERR_INVALID_ARGUMENT;char now[32];atena_now_iso8601(now);sqlite3_stmt*s=NULL;if(sqlite3_prepare_v2(store->db,"INSERT INTO audit_events(event,session_id,operation_id,details,created_at) VALUES(?,?,?,?,?)",-1,&s,NULL)!=SQLITE_OK)return ATENA_ERR_DB;sqlite3_bind_text(s,1,event,-1,SQLITE_TRANSIENT);if(session_id)sqlite3_bind_text(s,2,session_id,-1,SQLITE_TRANSIENT);else sqlite3_bind_null(s,2);if(operation_id)sqlite3_bind_text(s,3,operation_id,-1,SQLITE_TRANSIENT);else sqlite3_bind_null(s,3);sqlite3_bind_text(s,4,details?details:"",-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,5,now,-1,SQLITE_TRANSIENT);int rc=sqlite3_step(s);sqlite3_finalize(s);return rc==SQLITE_DONE?ATENA_OK:ATENA_ERR_DB;}
