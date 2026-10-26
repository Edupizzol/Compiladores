#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtab_dump.h"

#define N_COLS 6

static const char *kind_name(SymKind kind) {
    switch (kind) {
        case SYM_VAR:   return "variável";
        case SYM_PARAM: return "parâmetro";
        case SYM_FUNC:  return "função";
        default:        return "?";
    }
}

char *symbol_type_str(const Symbol *s, char *buf, size_t size) {
    if (s->kind != SYM_FUNC) {
        snprintf(buf, size, "%s", type_to_c(s->type));
        return buf;
    }
    size_t len = snprintf(buf, size, "%s (", type_to_c(s->type));
    if (s->n_params == 0)
        len += snprintf(buf + len, len < size ? size - len : 0, "void");
    for (int i = 0; i < s->n_params; i++)
        len += snprintf(buf + len, len < size ? size - len : 0, "%s%s",
                        i ? ", " : "", type_to_c(s->param_types[i]));
    snprintf(buf + len, len < size ? size - len : 0, ")");
    return buf;
}

/* largura na tela de uma string UTF-8 (strlen contaria o `Í` de NIVEL
   e o `â` de parametro como dois caracteres e desalinharia as colunas) */
static int display_width(const char *s) {
    int w = 0;
    for (; *s; s++)
        if (((unsigned char)*s & 0xC0) != 0x80) w++;
    return w;
}

static void print_row(FILE *out, char cells[N_COLS][128], const int *width) {
    for (int c = 0; c < N_COLS; c++) {
        fputs(cells[c], out);
        if (c < N_COLS - 1)
            fprintf(out, "%*s", width[c] - display_width(cells[c]), "");
    }
    fputc('\n', out);
}

static void fill_row(const Symbol *s, char cells[N_COLS][128]) {
    snprintf(cells[0], 128, "%d", s->scope_id);
    snprintf(cells[1], 128, "%d", s->level);
    snprintf(cells[2], 128, "%s", s->name);
    snprintf(cells[3], 128, "%s", kind_name(s->kind));
    symbol_type_str(s, cells[4], 128);
    snprintf(cells[5], 128, "%d", s->line);
}

void symtab_print(FILE *out, Symbol *const *syms, int n) {
    static const char *header[N_COLS] = { "ESCOPO", "NÍVEL", "NOME", "CATEGORIA", "TIPO", "LINHA" };

    /* insertion sort por scope_id: estavel, entao dentro de um escopo
       fica a ordem de declaracao (a ordem do array) */
    Symbol **sorted = malloc((n ? n : 1) * sizeof(Symbol *));
    for (int i = 0; i < n; i++) {
        int j = i;
        while (j > 0 && sorted[j - 1]->scope_id > syms[i]->scope_id) {
            sorted[j] = sorted[j - 1];
            j--;
        }
        sorted[j] = syms[i];
    }

    /* cada coluna tem a largura do maior valor (ou do cabecalho) + 2 */
    char cells[N_COLS][128];
    int width[N_COLS];
    for (int c = 0; c < N_COLS; c++)
        width[c] = display_width(header[c]);
    for (int i = 0; i < n; i++) {
        fill_row(sorted[i], cells);
        for (int c = 0; c < N_COLS; c++) {
            int w = display_width(cells[c]);
            if (w > width[c]) width[c] = w;
        }
    }
    for (int c = 0; c < N_COLS; c++)
        width[c] += 2;

    for (int c = 0; c < N_COLS; c++)
        snprintf(cells[c], 128, "%s", header[c]);
    print_row(out, cells, width);
    for (int i = 0; i < n; i++) {
        fill_row(sorted[i], cells);
        print_row(out, cells, width);
    }
    free(sorted);
}

typedef struct {
    Symbol **syms;
    int n, cap;
} SymArray;

static void collect(Symbol *sym, void *ctx) {
    SymArray *a = ctx;
    if (a->n == a->cap) {
        a->cap = a->cap ? a->cap * 2 : 16;
        a->syms = realloc(a->syms, a->cap * sizeof(Symbol *));
    }
    a->syms[a->n++] = sym;
}

void symtab_dump(FILE *out) {
    /* o symtab_foreach ja vem por escopo e, dentro dele, na ordem de
       declaracao; o symtab_print ordena de novo por scope_id, o que nao
       muda nada aqui (a ordenacao e estavel) */
    SymArray a = { NULL, 0, 0 };
    symtab_foreach(collect, &a);
    symtab_print(out, a.syms, a.n);
    free(a.syms);
}
