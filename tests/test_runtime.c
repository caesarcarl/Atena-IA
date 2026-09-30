#include "atena/runtime.h"
#include <stdio.h>

int main(void) {
    AtenaResourceSnapshot snapshot = {0};
    AtenaRuntimePlan plan = {0};
    AtenaStatus s = atena_runtime_snapshot(&snapshot);
    if (s != ATENA_OK) {
        fprintf(stderr, "runtime snapshot failed: %d\n", (int)s);
        return 2;
    }
    if (snapshot.ram_total_bytes == 0 || snapshot.logical_cpus == 0) return 3;
    s = atena_runtime_plan(&snapshot, &plan);
    if (s != ATENA_OK) return 4;
    if (plan.recommended_threads == 0 || plan.recommended_context_tokens == 0 ||
        plan.recommended_batch_tokens < 32 || !plan.profile[0]) return 5;
    printf("runtime profile=%s threads=%u context=%u batch=%u keep_alive=%u pressure=%s\n",
           plan.profile, plan.recommended_threads, plan.recommended_context_tokens,
           plan.recommended_batch_tokens, plan.recommended_keep_alive_seconds,
           atena_memory_pressure_string(snapshot.memory_pressure));
    return 0;
}
