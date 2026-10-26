#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic.h"
#include "symtab.h"

int semantic_errors = 0;

typedef struct {
    int line;
    char *msg;
} SemError;

static SemError *errors;
static int n_errors, cap_errors;

void semantic_error(int line, const char *fmt, ...) {
    va_list args, args_copy;
    va_start(args, fmt);
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    char *msg = malloc(len + 1);
    vsnprintf(msg, len + 1, fmt, args_copy);
    va_end(args_copy);

    if (n_errors == cap_errors) {
        cap_errors = cap_errors ? cap_errors * 2 : 8;
        errors = realloc(errors, cap_errors * sizeof(SemError));
    }
    errors[n_errors].line = line;
    errors[n_errors].msg = msg;
    n_errors++;
    semantic_errors++;
}

static void free_calls(void);

void semantic_flush(void) {
    /* insertion sort: estavel, entao a mesma linha mantem a ordem de registro */
    for (int i = 1; i < n_errors; i++) {
        SemError e = errors[i];
        int j = i - 1;
        for (; j >= 0 && errors[j].line > e.line; j--)
            errors[j + 1] = errors[j];
        errors[j + 1] = e;
    }
    for (int i = 0; i < n_errors; i++) {
        fprintf(stderr, "Erro semântico na linha %d: %s\n", errors[i].line, errors[i].msg);
        free(errors[i].msg);
    }
    free(errors);
    errors = NULL;
    n_errors = cap_errors = 0;

    /* com erro sintatico o semantic_check_calls nao chega a rodar */
    free_calls();
}

typedef struct {
    char *name;
    int n_args;
    int line;
} Call;

static Call *calls;
static int n_calls, cap_calls;

void semantic_call(const char *name, int n_args, int line) {
    if (n_calls == cap_calls) {
        cap_calls = cap_calls ? cap_calls * 2 : 8;
        calls = realloc(calls, cap_calls * sizeof(Call));
    }
    calls[n_calls].name = strdup(name);
    calls[n_calls].n_args = n_args;
    calls[n_calls].line = line;
    n_calls++;
}

void semantic_check_calls(void) {
    for (int i = 0; i < n_calls; i++) {
        Call *c = &calls[i];
        Symbol *f = symtab_lookup_func(c->name);
        if (!f)
            semantic_error(c->line, "função '%s' não declarada", c->name);
        else if (f->n_params != c->n_args)
            semantic_error(c->line, "função '%s' espera %d argumento(s), mas recebeu %d",
                           c->name, f->n_params, c->n_args);
    }
    free_calls();
}

static void free_calls(void) {
    for (int i = 0; i < n_calls; i++)
        free(calls[i].name);
    free(calls);
    calls = NULL;
    n_calls = cap_calls = 0;
}
