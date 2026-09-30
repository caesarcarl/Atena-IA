#ifndef ATENA_RUNTIME_H
#define ATENA_RUNTIME_H

#include <stdint.h>
#include "atena/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum AtenaMemoryPressure {
    ATENA_MEMORY_COMFORTABLE = 0,
    ATENA_MEMORY_MODERATE = 1,
    ATENA_MEMORY_TIGHT = 2,
    ATENA_MEMORY_CRITICAL = 3
} AtenaMemoryPressure;

typedef struct AtenaResourceSnapshot {
    uint64_t ram_total_bytes;
    uint64_t ram_available_bytes;
    uint64_t ram_free_bytes;
    uint64_t ram_cached_bytes;
    uint64_t ram_buffers_bytes;
    uint64_t swap_total_bytes;
    uint64_t swap_free_bytes;
    uint32_t logical_cpus;
    double load_1m;
    double load_5m;
    double load_15m;
    AtenaMemoryPressure memory_pressure;
    char architecture[32];
    char platform[32];
} AtenaResourceSnapshot;

typedef struct AtenaRuntimePlan {
    char profile[32];
    uint32_t recommended_threads;
    uint32_t recommended_context_tokens;
    uint32_t recommended_max_output_tokens;
    uint32_t recommended_batch_tokens;
    uint32_t recommended_keep_alive_seconds;
    uint32_t rag_level;
    int keep_model_resident;
    int allow_python_worker;
} AtenaRuntimePlan;

AtenaStatus atena_runtime_snapshot(AtenaResourceSnapshot *out_snapshot);
AtenaStatus atena_runtime_plan(const AtenaResourceSnapshot *snapshot, AtenaRuntimePlan *out_plan);
const char *atena_memory_pressure_string(AtenaMemoryPressure pressure);

#ifdef __cplusplus
}
#endif
#endif
