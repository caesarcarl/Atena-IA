#include "atena/pedagogy.h"
#include "test_common.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    AtenaPedagogyPlan plan;

    ATENA_TEST_ASSERT(atena_pedagogy_plan("Responda somente: OK", &plan) == ATENA_OK);
    ATENA_TEST_ASSERT(plan.mode == ATENA_PEDAGOGY_DIRECT);
    ATENA_TEST_ASSERT(plan.answer_first);
    ATENA_TEST_ASSERT(plan.question_budget == 0);

    ATENA_TEST_ASSERT(atena_pedagogy_plan(
        "Quero aprender ponteiros em C. Me guie passo a passo.", &plan) == ATENA_OK);
    ATENA_TEST_ASSERT(plan.mode == ATENA_PEDAGOGY_STUDY);
    ATENA_TEST_ASSERT(!plan.answer_first);
    ATENA_TEST_ASSERT(plan.progressive_hints);

    ATENA_TEST_ASSERT(atena_pedagogy_plan(
        "Não me dê a resposta; faça eu pensar.", &plan) == ATENA_OK);
    ATENA_TEST_ASSERT(plan.mode == ATENA_PEDAGOGY_STUDY);

    ATENA_TEST_ASSERT(atena_pedagogy_plan(
        "Como funciona um ponteiro em C?", &plan) == ATENA_OK);
    ATENA_TEST_ASSERT(plan.mode == ATENA_PEDAGOGY_ADAPTIVE);
    ATENA_TEST_ASSERT(plan.answer_first);

    ATENA_TEST_ASSERT(strcmp(atena_pedagogy_mode_name(plan.mode), "adaptive") == 0);
    ATENA_TEST_ASSERT(atena_pedagogy_instruction(&plan, 0) != NULL);
    ATENA_TEST_ASSERT(atena_pedagogy_instruction(&plan, 1) != NULL);

    puts("test_pedagogy: PASS");
    return 0;
}
