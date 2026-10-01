#include "atena/pedagogy.h"

#include <stddef.h>
#include <string.h>

static unsigned char ascii_lower(unsigned char c) {
    return (c >= 'A' && c <= 'Z') ? (unsigned char)(c + ('a' - 'A')) : c;
}

static int contains_ci(const char *text, const char *needle) {
    if (!text || !needle || !*needle) return 0;
    const size_t n = strlen(needle);
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        size_t i = 0;
        while (i < n && p[i] &&
               ascii_lower(p[i]) == ascii_lower((unsigned char)needle[i])) {
            ++i;
        }
        if (i == n) return 1;
    }
    return 0;
}

static int contains_any(const char *text, const char *const *items, size_t count) {
    for (size_t i = 0; i < count; ++i)
        if (contains_ci(text, items[i])) return 1;
    return 0;
}

static void set_direct(AtenaPedagogyPlan *p) {
    p->mode = ATENA_PEDAGOGY_DIRECT;
    p->question_budget = 0;
    p->answer_first = 1;
    p->progressive_hints = 0;
    p->check_understanding = 0;
}

static void set_adaptive(AtenaPedagogyPlan *p) {
    p->mode = ATENA_PEDAGOGY_ADAPTIVE;
    p->question_budget = 1;
    p->answer_first = 1;
    p->progressive_hints = 1;
    p->check_understanding = 1;
}

static void set_study(AtenaPedagogyPlan *p) {
    p->mode = ATENA_PEDAGOGY_STUDY;
    p->question_budget = 1;
    p->answer_first = 0;
    p->progressive_hints = 1;
    p->check_understanding = 1;
}

AtenaStatus atena_pedagogy_plan(const char *user_text, AtenaPedagogyPlan *out_plan) {
    if (!user_text || !out_plan) return ATENA_ERR_INVALID_ARGUMENT;
    set_adaptive(out_plan);

    static const char *strong_study[] = {
        "não me dê a resposta", "nao me de a resposta",
        "não entregue a resposta", "nao entregue a resposta",
        "faça eu pensar", "faca eu pensar"
    };
    if (contains_any(user_text, strong_study, sizeof(strong_study) / sizeof(strong_study[0]))) {
        set_study(out_plan);
        return ATENA_OK;
    }

    static const char *direct[] = {
        "responda somente", "responda apenas",
        "só a resposta", "so a resposta",
        "sem explicação", "sem explicacao",
        "direto ao ponto",
        "apenas o comando", "somente o comando",
        "me dê a resposta", "me de a resposta"
    };
    if (contains_any(user_text, direct, sizeof(direct) / sizeof(direct[0]))) {
        set_direct(out_plan);
        return ATENA_OK;
    }

    static const char *study[] = {
        "quero aprender", "quero entender",
        "quero estudar", "vamos estudar",
        "me ensine", "ensine-me",
        "me ajude a entender",
        "me guie", "guie-me",
        "passo a passo",
        "modo estudo", "modo de estudo",
        "método socrático", "metodo socratico"
    };
    if (contains_any(user_text, study, sizeof(study) / sizeof(study[0])))
        set_study(out_plan);

    return ATENA_OK;
}

const char *atena_pedagogy_mode_name(AtenaPedagogyMode mode) {
    switch (mode) {
        case ATENA_PEDAGOGY_DIRECT: return "direct";
        case ATENA_PEDAGOGY_STUDY: return "study";
        default: return "adaptive";
    }
}

const char *atena_pedagogy_instruction(const AtenaPedagogyPlan *plan, int compact) {
    if (!plan) return NULL;
    if (compact) {
        switch (plan->mode) {
            case ATENA_PEDAGOGY_DIRECT:
                return "[ATENA PEDAGOGY: direct]\nResponda ou execute diretamente. Nao bloqueie a tarefa com perguntas pedagogicas.\n";
            case ATENA_PEDAGOGY_STUDY:
                return "[ATENA PEDAGOGY: study]\nEnsine ativamente com uma pergunta ou pista por vez e respeite pedidos por resposta direta.\n";
            default:
                return "[ATENA PEDAGOGY: adaptive]\nAjude primeiro e explique passos verificaveis quando isso melhorar a compreensao.\n";
        }
    }
    switch (plan->mode) {
        case ATENA_PEDAGOGY_DIRECT:
            return "[ATENA PEDAGOGY MODE: direct]\nConclua a tarefa ou entregue a resposta solicitada primeiro. Nao use perguntas como barreira.\n";
        case ATENA_PEDAGOGY_STUDY:
            return "[ATENA PEDAGOGY MODE: study | SAPERE AUDE]\nEnsine para aumentar autonomia: diagnostico breve, no maximo uma pergunta orientadora por turno, pistas progressivas e explicacao verificavel. Se o usuario pedir resposta direta, entregue-a.\n";
        default:
            return "[ATENA PEDAGOGY MODE: adaptive]\nAjude imediatamente e explique conceitos e passos verificaveis quando houver valor didatico. Use no maximo uma pergunta orientadora por turno e nao transforme conversas comuns em quiz.\n";
    }
}
