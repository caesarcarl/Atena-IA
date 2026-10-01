#ifndef ATENA_PEDAGOGY_H
#define ATENA_PEDAGOGY_H

#include "atena/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum AtenaPedagogyMode {
    ATENA_PEDAGOGY_DIRECT = 0,
    ATENA_PEDAGOGY_ADAPTIVE = 1,
    ATENA_PEDAGOGY_STUDY = 2
} AtenaPedagogyMode;

typedef struct AtenaPedagogyPlan {
    AtenaPedagogyMode mode;
    unsigned int question_budget;
    int answer_first;
    int progressive_hints;
    int check_understanding;
} AtenaPedagogyPlan;

AtenaStatus atena_pedagogy_plan(const char *user_text, AtenaPedagogyPlan *out_plan);
const char *atena_pedagogy_mode_name(AtenaPedagogyMode mode);
const char *atena_pedagogy_instruction(const AtenaPedagogyPlan *plan, int compact);

#ifdef __cplusplus
}
#endif
#endif
