#ifndef ATENA_PROVIDER_H
#define ATENA_PROVIDER_H

#include <stddef.h>
#include <stdint.h>
#include "atena/status.h"
#include "atena/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ATENA_CAP_TEXT              (1ULL << 0)
#define ATENA_CAP_STREAMING         (1ULL << 1)
#define ATENA_CAP_VISION            (1ULL << 2)
#define ATENA_CAP_AUDIO             (1ULL << 3)
#define ATENA_CAP_EMBEDDINGS        (1ULL << 4)
#define ATENA_CAP_TOOLS             (1ULL << 5)
#define ATENA_CAP_JSON_SCHEMA       (1ULL << 6)
#define ATENA_CAP_REASONING_CONTROL (1ULL << 7)
#define ATENA_CAP_MODEL_LIST        (1ULL << 8)
#define ATENA_CAP_USAGE             (1ULL << 9)
#define ATENA_CAP_CANCELLATION      (1ULL << 10)

typedef enum AtenaCapabilityState {
    ATENA_CAPABILITY_UNKNOWN = 0,
    ATENA_CAPABILITY_UNSUPPORTED = 1,
    ATENA_CAPABILITY_SUPPORTED = 2
} AtenaCapabilityState;

typedef struct AtenaProviderRequest {
    const char *operation_id;
    const char *model;
    const AtenaMessage *messages;
    size_t message_count;
    size_t max_output_tokens;
    double temperature;
    double top_p;
    AtenaReasoningLevel reasoning;
} AtenaProviderRequest;

typedef struct AtenaProvider AtenaProvider;

typedef struct AtenaProviderVTable {
    AtenaStatus (*generate)(AtenaProvider *provider,
                            const AtenaProviderRequest *request,
                            AtenaEventCallback callback,
                            void *userdata);
    AtenaStatus (*cancel)(AtenaProvider *provider, const char *operation_id);
    void (*destroy)(AtenaProvider *provider);
} AtenaProviderVTable;

struct AtenaProvider {
    char id[64];
    char type[32];
    char model[128];
    uint64_t capabilities;
    uint64_t capabilities_known;
    const AtenaProviderVTable *vtable;
    void *impl;
};

AtenaCapabilityState atena_provider_capability(const AtenaProvider *provider, uint64_t capability);
AtenaProvider *atena_mock_provider_create(const char *id, const char *model);
AtenaProvider *atena_ollama_provider_create(const char *id, const char *base_url, const char *model);
AtenaStatus atena_ollama_provider_set_bearer_token(AtenaProvider *provider, const char *token);
AtenaStatus atena_ollama_provider_test_json(AtenaProvider *provider, char **out_json);
AtenaStatus atena_ollama_provider_models_json(AtenaProvider *provider, char **out_json);
AtenaStatus atena_ollama_provider_pull_model(AtenaProvider *provider, const char *model);
AtenaStatus atena_ollama_provider_remove_model(AtenaProvider *provider, const char *model);
AtenaProvider *atena_openai_compatible_provider_create(const char *id, const char *base_url, const char *model, const char *token);
AtenaStatus atena_openai_compatible_provider_test_json(AtenaProvider *provider, char **out_json);
AtenaStatus atena_openai_compatible_provider_models_json(AtenaProvider *provider, char **out_json);

#ifdef __cplusplus
}
#endif
#endif
