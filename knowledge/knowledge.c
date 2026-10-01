#include "atena/knowledge.h"

#include <ctype.h>
#include <errno.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ATENA_KNOWLEDGE_INSTALLED_DB
#define ATENA_KNOWLEDGE_INSTALLED_DB ""
#endif

#define ATENA_KNOWLEDGE_MAX_CANDIDATES 80
#define ATENA_KNOWLEDGE_MAX_TERMS 32
#define ATENA_KNOWLEDGE_MAX_TERM 96

struct AtenaKnowledgeStore {
    sqlite3 *db;
    char path[1024];
};

typedef struct QueryPlan {
    char pack[64];
    char terms[ATENA_KNOWLEDGE_MAX_TERMS][ATENA_KNOWLEDGE_MAX_TERM];
    size_t term_count;
    int prefer_c_family;
    int prefer_python;
    int prefer_algorithms;
    int prefer_study_math;
} QueryPlan;

typedef struct Candidate {
    AtenaKnowledgeHit hit;
    double lexical_rank;
    char quality_flag[64];
} Candidate;

static char *dup_text(const unsigned char *value) {
    const char *s = value ? (const char *)value : "";
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1U);
    if (!out) return NULL;
    memcpy(out, s, n + 1U);
    return out;
}

static int file_exists(const char *path) {
    if (!path || !*path) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static int env_false(const char *v) {
    if (!v || !*v) return 0;
    return !strcmp(v, "0") || !strcmp(v, "off") || !strcmp(v, "false") || !strcmp(v, "no");
}

static int contains_ascii_ci(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle) return 0;
    size_t n = strlen(needle);
    for (const unsigned char *p = (const unsigned char *)haystack; *p; ++p) {
        size_t i = 0;
        while (i < n && p[i]) {
            unsigned char a = p[i], b = (unsigned char)needle[i];
            if (a >= 'A' && a <= 'Z') a = (unsigned char)(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z') b = (unsigned char)(b + ('a' - 'A'));
            if (a != b) break;
            ++i;
        }
        if (i == n) return 1;
    }
    return 0;
}

static int is_stopword(const char *t) {
    static const char *const words[] = {
        "a","ao","aos","as","com","como","da","das","de","do","dos","e","em",
        "na","nas","no","nos","o","os","ou","para","por","que","se","um","uma",
        "the","an","and","or","of","in","on","to","for","with","what","how","why","is","are"
    };
    for (size_t i = 0; i < sizeof(words)/sizeof(words[0]); ++i)
        if (!strcmp(t, words[i])) return 1;
    return 0;
}

static void normalize_token(const char *src, char out[ATENA_KNOWLEDGE_MAX_TERM]) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)src; *p && n + 1U < ATENA_KNOWLEDGE_MAX_TERM; ++p) {
        unsigned char c = *p;
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + ('a' - 'A'));
        if (isalnum(c) || c == '_' || c == '+' || c >= 0x80) out[n++] = (char)c;
    }
    out[n] = '\0';
}

static int plan_has_term(const QueryPlan *plan, const char *term) {
    for (size_t i = 0; i < plan->term_count; ++i)
        if (!strcmp(plan->terms[i], term)) return 1;
    return 0;
}

static void plan_add_term(QueryPlan *plan, const char *term) {
    if (!plan || !term || !*term || plan->term_count >= ATENA_KNOWLEDGE_MAX_TERMS) return;
    char normalized[ATENA_KNOWLEDGE_MAX_TERM];
    normalize_token(term, normalized);
    if (!*normalized || is_stopword(normalized) || plan_has_term(plan, normalized)) return;
    snprintf(plan->terms[plan->term_count], ATENA_KNOWLEDGE_MAX_TERM, "%s", normalized);
    plan->term_count++;
}

static void add_expansions(QueryPlan *plan, const char *term) {
    if (!strcmp(term, "ponteiro") || !strcmp(term, "ponteiros")) {
        plan_add_term(plan, "pointer"); plan_add_term(plan, "pointers"); plan_add_term(plan, "address");
    } else if (!strcmp(term, "malloc")) {
        plan_add_term(plan, "calloc"); plan_add_term(plan, "realloc"); plan_add_term(plan, "free");
        plan_add_term(plan, "dynamic"); plan_add_term(plan, "memory");
    } else if (!strcmp(term, "memoria") || !strcmp(term, "memória")) {
        plan_add_term(plan, "memory");
    } else if (!strcmp(term, "dinamica") || !strcmp(term, "dinâmica")) {
        plan_add_term(plan, "dynamic");
    } else if (!strcmp(term, "busca")) {
        plan_add_term(plan, "search");
    } else if (!strcmp(term, "largura")) {
        plan_add_term(plan, "breadth"); plan_add_term(plan, "bfs"); plan_add_term(plan, "queue"); plan_add_term(plan, "graph");
    } else if (!strcmp(term, "algoritmo") || !strcmp(term, "algoritmos")) {
        plan_add_term(plan, "algorithm"); plan_add_term(plan, "algorithms");
    } else if (!strcmp(term, "relacoes") || !strcmp(term, "relações") || !strcmp(term, "relacao") || !strcmp(term, "relação")) {
        plan_add_term(plan, "relation"); plan_add_term(plan, "relations");
    } else if (!strcmp(term, "funcoes") || !strcmp(term, "funções") || !strcmp(term, "funcao") || !strcmp(term, "função")) {
        plan_add_term(plan, "function"); plan_add_term(plan, "functions");
    } else if (!strcmp(term, "conjuntos") || !strcmp(term, "conjunto")) {
        plan_add_term(plan, "set"); plan_add_term(plan, "sets");
    } else if (!strcmp(term, "matematica") || !strcmp(term, "matemática")) {
        plan_add_term(plan, "mathematics"); plan_add_term(plan, "math");
    } else if (!strcmp(term, "discreta")) {
        plan_add_term(plan, "discrete");
    } else if (!strcmp(term, "retorna") || !strcmp(term, "retornam")) {
        plan_add_term(plan, "return"); plan_add_term(plan, "returns");
    } else if (!strcmp(term, "valor") || !strcmp(term, "valores")) {
        plan_add_term(plan, "value"); plan_add_term(plan, "values");
    }
}

static void parse_query_terms(QueryPlan *plan, const char *query) {
    char token[ATENA_KNOWLEDGE_MAX_TERM];
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)query;; ++p) {
        unsigned char c = *p;
        int accepted = c && (isalnum(c) || c == '_' || c == '+' || c >= 0x80);
        if (accepted && n + 1U < sizeof(token)) {
            token[n++] = (char)c;
            continue;
        }
        if (n) {
            token[n] = '\0';
            char norm[ATENA_KNOWLEDGE_MAX_TERM];
            normalize_token(token, norm);
            if (*norm && !is_stopword(norm) && (strlen(norm) >= 2U || !strcmp(norm, "c"))) {
                plan_add_term(plan, norm);
                add_expansions(plan, norm);
            }
            n = 0;
        }
        if (!c) break;
    }
}

static void build_query_plan(const char *query, QueryPlan *plan) {
    memset(plan, 0, sizeof(*plan));
    snprintf(plan->pack, sizeof(plan->pack), "%s", atena_knowledge_route_pack(query));
    parse_query_terms(plan, query);

    if (contains_ascii_ci(query, "malloc") || contains_ascii_ci(query, "ponteiro") || contains_ascii_ci(query, "pointer")) {
        plan->prefer_c_family = 1;
        plan_add_term(plan, "malloc"); plan_add_term(plan, "pointer"); plan_add_term(plan, "memory"); plan_add_term(plan, "free");
    }
    if (contains_ascii_ci(query, "python")) plan->prefer_python = 1;
    if (contains_ascii_ci(query, "algoritmo") || contains_ascii_ci(query, "breadth") || contains_ascii_ci(query, "bfs") ||
        (contains_ascii_ci(query, "busca") && contains_ascii_ci(query, "largura"))) {
        plan->prefer_algorithms = 1;
        plan_add_term(plan, "breadth"); plan_add_term(plan, "bfs"); plan_add_term(plan, "graph"); plan_add_term(plan, "queue");
    }
    if (contains_ascii_ci(query, "matemat") || contains_ascii_ci(query, "conjunto") || contains_ascii_ci(query, "rela") || contains_ascii_ci(query, "discreta")) {
        plan->prefer_study_math = 1;
        plan_add_term(plan, "set"); plan_add_term(plan, "sets"); plan_add_term(plan, "relation");
        plan_add_term(plan, "relations"); plan_add_term(plan, "function"); plan_add_term(plan, "functions");
    }
}

static AtenaStatus make_fts_expression(const QueryPlan *plan, char **out_expr) {
    if (!plan || !out_expr) return ATENA_ERR_INVALID_ARGUMENT;
    *out_expr = NULL;
    if (!plan->term_count) return ATENA_ERR_NOT_FOUND;
    size_t cap = 64U + plan->term_count * (ATENA_KNOWLEDGE_MAX_TERM + 8U);
    char *buf = (char *)malloc(cap);
    if (!buf) return ATENA_ERR_NO_MEMORY;
    size_t len = 0;
    for (size_t i = 0; i < plan->term_count; ++i) {
        int w = snprintf(buf + len, cap - len, "%s\"%s\"", i ? " OR " : "", plan->terms[i]);
        if (w < 0 || (size_t)w >= cap - len) { free(buf); return ATENA_ERR_INTERNAL; }
        len += (size_t)w;
    }
    *out_expr = buf;
    return ATENA_OK;
}

static size_t matched_terms(const QueryPlan *plan, const char *title, const char *content) {
    size_t matched = 0;
    for (size_t i = 0; i < plan->term_count; ++i)
        if (contains_ascii_ci(title, plan->terms[i]) || contains_ascii_ci(content, plan->terms[i])) matched++;
    return matched;
}

static double score_candidate(const QueryPlan *plan, Candidate *candidate) {
    AtenaKnowledgeHit *h = &candidate->hit;
    double score = candidate->lexical_rank < 0.0 ? -candidate->lexical_rank : 0.0;
    size_t matched = matched_terms(plan, h->title ? h->title : "", h->content ? h->content : "");
    size_t denom = plan->term_count < 10U ? plan->term_count : 10U;
    h->coverage = denom ? (double)matched / (double)denom : 0.0;
    if (h->coverage > 1.0) h->coverage = 1.0;
    score += (double)matched * 1.8;

    if (plan->prefer_c_family) {
        if (!strncmp(h->source_id, "dev.c.", 6) || !strncmp(h->source_id, "dev.cpp.", 8)) score += 16.0;
        if (!strncmp(h->source_id, "dev.python.", 11)) score -= 14.0;
        if (contains_ascii_ci(h->content, "malloc")) score += 18.0;
        if (contains_ascii_ci(h->content, "pointer")) score += 8.0;
        if (contains_ascii_ci(h->content, "dynamic memory")) score += 7.0;
    }
    if (plan->prefer_python && !strncmp(h->source_id, "dev.python.", 11)) score += 16.0;
    if (plan->prefer_algorithms) {
        if (!strncmp(h->source_id, "dev.algorithms.", 15)) score += 18.0;
        if (contains_ascii_ci(h->content, "breadth-first search") || contains_ascii_ci(h->content, "breadth first search") || contains_ascii_ci(h->content, "bfs")) score += 18.0;
    }
    if (plan->prefer_study_math) {
        if (!strncmp(h->source_id, "study.math.", 11)) score += 18.0;
        if (contains_ascii_ci(h->content, "sets") || contains_ascii_ci(h->content, "relations") || contains_ascii_ci(h->content, "functions")) score += 8.0;
    }
    if (!strcmp(candidate->quality_flag, "short_textbook_review")) score -= 6.0;

    h->score = score;
    if (score >= 30.0 && h->coverage >= 0.20) h->confidence = ATENA_KNOWLEDGE_CONFIDENCE_HIGH;
    else if (score >= 16.0 && h->coverage >= 0.10) h->confidence = ATENA_KNOWLEDGE_CONFIDENCE_MEDIUM;
    else h->confidence = ATENA_KNOWLEDGE_CONFIDENCE_LOW;
    return score;
}

static int candidate_cmp(const void *a, const void *b) {
    const Candidate *ca = (const Candidate *)a;
    const Candidate *cb = (const Candidate *)b;
    if (ca->hit.score > cb->hit.score) return -1;
    if (ca->hit.score < cb->hit.score) return 1;
    if (ca->hit.coverage > cb->hit.coverage) return -1;
    if (ca->hit.coverage < cb->hit.coverage) return 1;
    return 0;
}

static void candidate_free(Candidate *c) {
    if (!c) return;
    free(c->hit.title); free(c->hit.locator); free(c->hit.content);
    memset(c, 0, sizeof(*c));
}

static AtenaStatus validate_schema(sqlite3 *db) {
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT count(*) FROM chunks_fts JOIN chunks c ON c.id=chunks_fts.rowid JOIN documents d ON d.id=c.document_id LIMIT 1";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return ATENA_ERR_DB;
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_ROW || rc == SQLITE_DONE) ? ATENA_OK : ATENA_ERR_DB;
}

AtenaStatus atena_knowledge_open(const char *path, AtenaKnowledgeStore **out_store) {
    if (!path || !*path || !out_store) return ATENA_ERR_INVALID_ARGUMENT;
    *out_store = NULL;
    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, NULL);
    if (rc != SQLITE_OK) { if (db) sqlite3_close(db); return rc == SQLITE_CANTOPEN ? ATENA_ERR_NOT_FOUND : ATENA_ERR_DB; }
    sqlite3_busy_timeout(db, 1500);
    AtenaStatus st = validate_schema(db);
    if (st != ATENA_OK) { sqlite3_close(db); return st; }
    AtenaKnowledgeStore *store = (AtenaKnowledgeStore *)calloc(1, sizeof(*store));
    if (!store) { sqlite3_close(db); return ATENA_ERR_NO_MEMORY; }
    store->db = db;
    snprintf(store->path, sizeof(store->path), "%s", path);
    *out_store = store;
    return ATENA_OK;
}

AtenaStatus atena_knowledge_open_default(AtenaKnowledgeStore **out_store) {
    if (!out_store) return ATENA_ERR_INVALID_ARGUMENT;
    *out_store = NULL;
    const char *explicit_path = getenv("ATENA_KNOWLEDGE_DB");
    if (explicit_path && *explicit_path) {
        if (env_false(explicit_path)) return ATENA_ERR_NOT_FOUND;
        return atena_knowledge_open(explicit_path, out_store);
    }

    char path[1024];
    const char *xdg = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    if (xdg && *xdg) {
        int n = snprintf(path, sizeof(path), "%s/atena/knowledge/index/knowledge.sqlite", xdg);
        if (n > 0 && (size_t)n < sizeof(path) && file_exists(path)) return atena_knowledge_open(path, out_store);
    }
    if (home && *home) {
        int n = snprintf(path, sizeof(path), "%s/.local/share/atena/knowledge/index/knowledge.sqlite", home);
        if (n > 0 && (size_t)n < sizeof(path) && file_exists(path)) return atena_knowledge_open(path, out_store);
    }
    if (*ATENA_KNOWLEDGE_INSTALLED_DB && file_exists(ATENA_KNOWLEDGE_INSTALLED_DB))
        return atena_knowledge_open(ATENA_KNOWLEDGE_INSTALLED_DB, out_store);
    return ATENA_ERR_NOT_FOUND;
}

void atena_knowledge_close(AtenaKnowledgeStore *store) {
    if (!store) return;
    if (store->db) sqlite3_close(store->db);
    free(store);
}

AtenaStatus atena_knowledge_stats(AtenaKnowledgeStore *store, AtenaKnowledgeStats *out_stats) {
    if (!out_stats) return ATENA_ERR_INVALID_ARGUMENT;
    memset(out_stats, 0, sizeof(*out_stats));
    if (!store || !store->db) return ATENA_ERR_NOT_FOUND;
    out_stats->available = 1;
    snprintf(out_stats->path, sizeof(out_stats->path), "%s", store->path);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(store->db, "SELECT (SELECT count(*) FROM documents),(SELECT count(*) FROM chunks)", -1, &stmt, NULL) != SQLITE_OK) return ATENA_ERR_DB;
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        out_stats->documents = sqlite3_column_int64(stmt, 0);
        out_stats->chunks = sqlite3_column_int64(stmt, 1);
    }
    sqlite3_finalize(stmt);
    return rc == SQLITE_ROW ? ATENA_OK : ATENA_ERR_DB;
}

const char *atena_knowledge_route_pack(const char *query) {
    if (!query) return "atena.common";
    if (contains_ascii_ci(query,"python") || contains_ascii_ci(query,"malloc") || contains_ascii_ci(query,"ponteiro") ||
        contains_ascii_ci(query,"c++") || contains_ascii_ci(query,"linux") || contains_ascii_ci(query,"git") ||
        contains_ascii_ci(query,"algorit") || contains_ascii_ci(query,"bash") || contains_ascii_ci(query,"shell") ||
        contains_ascii_ci(query,"sql") || contains_ascii_ci(query,"api") || contains_ascii_ci(query,"debug")) return "atena.dev";
    if (contains_ascii_ci(query,"matemat") || contains_ascii_ci(query,"fisic") || contains_ascii_ci(query,"quimic") ||
        contains_ascii_ci(query,"biolog") || contains_ascii_ci(query,"historia") || contains_ascii_ci(query,"geograf") ||
        contains_ascii_ci(query,"redacao") || contains_ascii_ci(query,"redação") || contains_ascii_ci(query,"vestibular") ||
        contains_ascii_ci(query,"enem") || contains_ascii_ci(query,"conjunto") || contains_ascii_ci(query,"rela") ||
        contains_ascii_ci(query,"derivada") || contains_ascii_ci(query,"integral")) return "atena.study";
    if (contains_ascii_ci(query,"email") || contains_ascii_ci(query,"planilha") || contains_ascii_ci(query,"documento") ||
        contains_ascii_ci(query,"wifi") || contains_ascii_ci(query,"wi-fi") || contains_ascii_ci(query,"impressora") ||
        contains_ascii_ci(query,"celular") || contains_ascii_ci(query,"backup") || contains_ascii_ci(query,"navegador") ||
        contains_ascii_ci(query,"arquivo") || contains_ascii_ci(query,"pasta")) return "atena.everyday";
    return "atena.common";
}

AtenaStatus atena_knowledge_search(AtenaKnowledgeStore *store,
                                   const char *query,
                                   size_t limit,
                                   AtenaKnowledgeHit **out_hits,
                                   size_t *out_count) {
    if (!store || !store->db || !query || !*query || !out_hits || !out_count) return ATENA_ERR_INVALID_ARGUMENT;
    *out_hits = NULL; *out_count = 0;
    if (!limit) limit = 3;
    if (limit > 12U) limit = 12U;

    QueryPlan plan;
    build_query_plan(query, &plan);
    char *fts = NULL;
    AtenaStatus st = make_fts_expression(&plan, &fts);
    if (st != ATENA_OK) return st;

    const char *sql =
        "SELECT c.chunk_uid,c.source_id,c.pack_id,c.kind,c.title,c.locator,c.content,c.content_sha256,"
        "d.quality_flag,bm25(chunks_fts,2.0,1.0) "
        "FROM chunks_fts JOIN chunks c ON c.id=chunks_fts.rowid "
        "JOIN documents d ON d.id=c.document_id "
        "WHERE chunks_fts MATCH ?1 AND c.retrieval_policy='answer_evidence' "
        "AND (c.pack_id=?2 OR c.pack_id='atena.common') "
        "ORDER BY bm25(chunks_fts,2.0,1.0) LIMIT ?3";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL) != SQLITE_OK) { free(fts); return ATENA_ERR_DB; }
    sqlite3_bind_text(stmt, 1, fts, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, plan.pack, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, ATENA_KNOWLEDGE_MAX_CANDIDATES);
    free(fts);

    Candidate *candidates = (Candidate *)calloc(ATENA_KNOWLEDGE_MAX_CANDIDATES, sizeof(*candidates));
    if (!candidates) { sqlite3_finalize(stmt); return ATENA_ERR_NO_MEMORY; }
    size_t count = 0;
    int rc;
    while (count < ATENA_KNOWLEDGE_MAX_CANDIDATES && (rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        Candidate *c = &candidates[count];
        const unsigned char *chunk_uid = sqlite3_column_text(stmt,0);
        const unsigned char *source_id = sqlite3_column_text(stmt,1);
        const unsigned char *pack_id = sqlite3_column_text(stmt,2);
        const unsigned char *kind = sqlite3_column_text(stmt,3);
        const unsigned char *title = sqlite3_column_text(stmt,4);
        const unsigned char *locator = sqlite3_column_text(stmt,5);
        const unsigned char *content = sqlite3_column_text(stmt,6);
        const unsigned char *sha = sqlite3_column_text(stmt,7);
        const unsigned char *quality = sqlite3_column_text(stmt,8);
        snprintf(c->hit.chunk_uid,sizeof(c->hit.chunk_uid),"%s",chunk_uid?(const char*)chunk_uid:"");
        snprintf(c->hit.source_id,sizeof(c->hit.source_id),"%s",source_id?(const char*)source_id:"");
        snprintf(c->hit.pack_id,sizeof(c->hit.pack_id),"%s",pack_id?(const char*)pack_id:"");
        snprintf(c->hit.kind,sizeof(c->hit.kind),"%s",kind?(const char*)kind:"");
        snprintf(c->hit.content_sha256,sizeof(c->hit.content_sha256),"%s",sha?(const char*)sha:"");
        snprintf(c->quality_flag,sizeof(c->quality_flag),"%s",quality?(const char*)quality:"");
        c->hit.title = dup_text(title); c->hit.locator = dup_text(locator); c->hit.content = dup_text(content);
        if (!c->hit.title || !c->hit.locator || !c->hit.content) {
            candidate_free(c);
            for (size_t i=0;i<count;i++) candidate_free(&candidates[i]);
            free(candidates); sqlite3_finalize(stmt); return ATENA_ERR_NO_MEMORY;
        }
        c->lexical_rank = sqlite3_column_double(stmt,9);
        score_candidate(&plan,c);
        count++;
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        for (size_t i=0;i<count;i++) candidate_free(&candidates[i]);
        free(candidates); return ATENA_ERR_DB;
    }
    if (!count) { free(candidates); return ATENA_OK; }

    qsort(candidates,count,sizeof(*candidates),candidate_cmp);
    AtenaKnowledgeHit *hits = (AtenaKnowledgeHit *)calloc(limit,sizeof(*hits));
    if (!hits) { for (size_t i=0;i<count;i++) candidate_free(&candidates[i]); free(candidates); return ATENA_ERR_NO_MEMORY; }
    size_t accepted = 0;
    for (size_t i=0;i<count && accepted<limit;i++) {
        if (candidates[i].hit.confidence < ATENA_KNOWLEDGE_CONFIDENCE_MEDIUM) continue;
        hits[accepted] = candidates[i].hit;
        candidates[i].hit.title = NULL; candidates[i].hit.locator = NULL; candidates[i].hit.content = NULL;
        accepted++;
    }
    for (size_t i=0;i<count;i++) candidate_free(&candidates[i]);
    free(candidates);
    if (!accepted) { free(hits); return ATENA_OK; }
    *out_hits = hits; *out_count = accepted;
    return ATENA_OK;
}

void atena_knowledge_hits_free(AtenaKnowledgeHit *hits, size_t count) {
    if (!hits) return;
    for (size_t i=0;i<count;i++) { free(hits[i].title); free(hits[i].locator); free(hits[i].content); }
    free(hits);
}

const char *atena_knowledge_confidence_name(AtenaKnowledgeConfidence confidence) {
    switch (confidence) {
        case ATENA_KNOWLEDGE_CONFIDENCE_HIGH: return "high";
        case ATENA_KNOWLEDGE_CONFIDENCE_MEDIUM: return "medium";
        default: return "low";
    }
}
