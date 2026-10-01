#include "atena/knowledge.h"
#include "test_common.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void exec_sql(sqlite3 *db, const char *sql) {
    char *error = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &error);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "sqlite error: %s\n", error ? error : "unknown");
        sqlite3_free(error);
        abort();
    }
}

int main(void) {
    char path[] = "/tmp/atena-knowledge-XXXXXX";
    int fd = mkstemp(path);
    ATENA_TEST_ASSERT(fd >= 0);
    close(fd);
    unlink(path);

    sqlite3 *db = NULL;
    ATENA_TEST_ASSERT(sqlite3_open(path, &db) == SQLITE_OK);
    exec_sql(db,
        "CREATE TABLE documents(id INTEGER PRIMARY KEY,source_id TEXT,quality_flag TEXT);"
        "CREATE TABLE chunks(id INTEGER PRIMARY KEY,document_id INTEGER,chunk_uid TEXT,source_id TEXT,pack_id TEXT,kind TEXT,title TEXT,locator TEXT,retrieval_policy TEXT,content_sha256 TEXT,content TEXT);"
        "CREATE VIRTUAL TABLE chunks_fts USING fts5(title,content,source_id UNINDEXED,pack_id UNINDEXED,kind UNINDEXED,retrieval_policy UNINDEXED,content='chunks',content_rowid='id',tokenize='unicode61 remove_diacritics 2');"
        "CREATE TRIGGER chunks_ai AFTER INSERT ON chunks BEGIN INSERT INTO chunks_fts(rowid,title,content,source_id,pack_id,kind,retrieval_policy) VALUES(new.id,new.title,new.content,new.source_id,new.pack_id,new.kind,new.retrieval_policy); END;"
        "INSERT INTO documents VALUES(1,'dev.python.synthetic','ok');"
        "INSERT INTO documents VALUES(2,'dev.c.synthetic','ok');"
        "INSERT INTO documents VALUES(3,'dev.algorithms.synthetic','ok');"
        "INSERT INTO documents VALUES(4,'study.math.synthetic','ok');"
        "INSERT INTO chunks VALUES(1,1,'p1','dev.python.synthetic','atena.dev','textbook','Python','page:1','answer_evidence','x','Python functions return values and the runtime manages memory.');"
        "INSERT INTO chunks VALUES(2,2,'c1','dev.c.synthetic','atena.dev','textbook','C Memory','page:1','answer_evidence','x','A pointer stores an address. malloc allocates dynamic memory and free releases it.');"
        "INSERT INTO chunks VALUES(3,3,'a1','dev.algorithms.synthetic','atena.dev','textbook','Algorithms','page:1','answer_evidence','x','Breadth first search BFS explores a graph level by level using a queue.');"
        "INSERT INTO chunks VALUES(4,4,'m1','study.math.synthetic','atena.study','textbook','Discrete Mathematics','page:1','answer_evidence','x','Sets, relations, and functions are fundamental objects in discrete mathematics.');");
    sqlite3_close(db);

    AtenaKnowledgeStore *store = NULL;
    ATENA_TEST_ASSERT(atena_knowledge_open(path, &store) == ATENA_OK);

    AtenaKnowledgeStats stats;
    ATENA_TEST_ASSERT(atena_knowledge_stats(store, &stats) == ATENA_OK);
    ATENA_TEST_ASSERT(stats.documents == 4);
    ATENA_TEST_ASSERT(stats.chunks == 4);

    struct Case { const char *query; const char *source_prefix; const char *pack; } cases[] = {
        {"ponteiros malloc memória dinâmica em C", "dev.c.", "atena.dev"},
        {"como funções retornam valores em Python", "dev.python.", "atena.dev"},
        {"algoritmo de busca em largura", "dev.algorithms.", "atena.dev"},
        {"relações funções conjuntos matemática discreta", "study.math.", "atena.study"}
    };

    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        ATENA_TEST_ASSERT(!strcmp(atena_knowledge_route_pack(cases[i].query), cases[i].pack));
        AtenaKnowledgeHit *hits = NULL;
        size_t count = 0;
        ATENA_TEST_ASSERT(atena_knowledge_search(store, cases[i].query, 3, &hits, &count) == ATENA_OK);
        ATENA_TEST_ASSERT(count > 0);
        ATENA_TEST_ASSERT(!strncmp(hits[0].source_id, cases[i].source_prefix, strlen(cases[i].source_prefix)));
        ATENA_TEST_ASSERT(hits[0].confidence >= ATENA_KNOWLEDGE_CONFIDENCE_MEDIUM);
        atena_knowledge_hits_free(hits, count);
    }

    atena_knowledge_close(store);
    unlink(path);
    puts("test_knowledge: PASS");
    return 0;
}
