#ifndef ATENA_KNOWLEDGE_H
#define ATENA_KNOWLEDGE_H

#include <stddef.h>
#include "atena/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AtenaKnowledgeStore AtenaKnowledgeStore;

typedef enum AtenaKnowledgeConfidence {
    ATENA_KNOWLEDGE_CONFIDENCE_LOW = 0,
    ATENA_KNOWLEDGE_CONFIDENCE_MEDIUM = 1,
    ATENA_KNOWLEDGE_CONFIDENCE_HIGH = 2
} AtenaKnowledgeConfidence;

typedef struct AtenaKnowledgeHit {
    char chunk_uid[65];
    char source_id[160];
    char pack_id[64];
    char kind[48];
    char content_sha256[72];
    char *title;
    char *locator;
    char *content;
    double score;
    double coverage;
    AtenaKnowledgeConfidence confidence;
} AtenaKnowledgeHit;

typedef struct AtenaKnowledgeStats {
    int available;
    long long documents;
    long long chunks;
    char path[1024];
} AtenaKnowledgeStats;

AtenaStatus atena_knowledge_open(const char *path, AtenaKnowledgeStore **out_store);
AtenaStatus atena_knowledge_open_default(AtenaKnowledgeStore **out_store);
void atena_knowledge_close(AtenaKnowledgeStore *store);
AtenaStatus atena_knowledge_stats(AtenaKnowledgeStore *store, AtenaKnowledgeStats *out_stats);
const char *atena_knowledge_route_pack(const char *query);
AtenaStatus atena_knowledge_search(AtenaKnowledgeStore *store,
                                   const char *query,
                                   size_t limit,
                                   AtenaKnowledgeHit **out_hits,
                                   size_t *out_count);
void atena_knowledge_hits_free(AtenaKnowledgeHit *hits, size_t count);
const char *atena_knowledge_confidence_name(AtenaKnowledgeConfidence confidence);

#ifdef __cplusplus
}
#endif

#endif
