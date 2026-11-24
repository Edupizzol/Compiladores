#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "optimize.h"
#include "semantic.h"

/* valor de um literal: inteiro (int de C) ou ponto flutuante (double) */
typedef struct {
    int is_float;
    long long i;
    double f;
} Lit;

/* 1 se `e` e um `expr` literal (um filho so, INT_NUMBER ou FLOAT_NUMBER).
   O valor sai do lexema da folha, que e o do fonte ou o de um folding
   anterior. Inteiro que nao cabe num int de C nao conta como literal */
static int literal_value(const Node *e, Lit *out) {
    if (e->n_children != 1) return 0;
    const Node *leaf = e->children[0];
    if (!leaf->text) return 0;
    char *end;
    errno = 0;
    if (strcmp(leaf->label, "INT_NUMBER") == 0) {
        long long v = strtoll(leaf->text, &end, 10);
        if (errno || *end || v < INT_MIN || v > INT_MAX) return 0;
        out->is_float = 0;
        out->i = v;
        return 1;
    }
    if (strcmp(leaf->label, "FLOAT_NUMBER") == 0) {
        double v = strtod(leaf->text, &end);
        if (errno || *end) return 0;
        out->is_float = 1;
        out->f = v;
        return 1;
    }
    return 0;
}

static double as_double(Lit v) { return v.is_float ? v.f : (double)v.i; }
static int is_true(Lit v) { return v.is_float ? v.f != 0.0 : v.i != 0; }

/* troca os filhos de `e` por uma folha so, marcada com `folded` (FOLD_*).
   O no continua sendo o mesmo, entao o pai nao precisa saber da troca */
static void replace_with_leaf(Node *e, const char *label, const char *text, int folded) {
    for (int i = 0; i < e->n_children; i++)
        node_free(e->children[i]);
    e->n_children = 0;
    node_add_child(e, node_leaf(label, text, e->line));
    e->folded = folded;
}

/* resultado inteiro (int de C). `rust_bool` marca comparacao e operador
   logico: em C dao 0/1, mas no Rust gerado sao bool, entao o codegen
   escreve true/false (o mesmo tipo que a expressao original tinha no Rust) */
static void set_int(Node *e, long long v, int rust_bool) {
    char text[32];
    snprintf(text, sizeof text, "%lld", v);
    replace_with_leaf(e, "INT_NUMBER", text, rust_bool ? FOLD_BOOL : FOLD_VALUE);
    e->type = TYPE_INT;
}

/* resultado double: o texto mais curto que volta pro mesmo valor, sempre
   com ponto ou expoente pra continuar sendo literal float em C e em Rust */
static void set_float(Node *e, double v) {
    char text[64];
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(text, sizeof text, "%.*g", prec, v);
        if (strtod(text, NULL) == v) break;
    }
    if (!strpbrk(text, ".e")) strcat(text, ".0");
    replace_with_leaf(e, "FLOAT_NUMBER", text, FOLD_VALUE);
}

static int fits_int(long long v) { return v >= INT_MIN && v <= INT_MAX; }

/* `-x` e `!x` */
static void fold_unary(Node *e) {
    Lit v;
    if (!literal_value(e->children[1], &v)) return;
    const char *op = e->children[0]->label;
    if (strcmp(op, "NOT") == 0) {
        set_int(e, !is_true(v), 1);
    } else if (strcmp(op, "MINUS") == 0) {
        if (v.is_float) set_float(e, -v.f);
        else if (v.i != INT_MIN) set_int(e, -v.i, 0);   /* -INT_MIN estoura */
    }
}

/* comparacao: da 0/1 tanto com int quanto com double */
static int compare(const char *op, Lit a, Lit b, int *out) {
    int fl = a.is_float || b.is_float;
    double x = as_double(a), y = as_double(b);
    long long i = a.i, j = b.i;
    if (strcmp(op, "COMP") == 0)    *out = fl ? x == y : i == j;
    else if (strcmp(op, "NE") == 0) *out = fl ? x != y : i != j;
    else if (strcmp(op, "LT") == 0) *out = fl ? x < y : i < j;
    else if (strcmp(op, "GT") == 0) *out = fl ? x > y : i > j;
    else if (strcmp(op, "LE") == 0) *out = fl ? x <= y : i <= j;
    else if (strcmp(op, "GE") == 0) *out = fl ? x >= y : i >= j;
    else return 0;
    return 1;
}

/* aritmetica em ponto flutuante (algum lado e float/double) */
static void fold_float_arith(Node *e, const char *op, double x, double y) {
    double r;
    if (strcmp(op, "PLUS") == 0)       r = x + y;
    else if (strcmp(op, "MINUS") == 0) r = x - y;
    else if (strcmp(op, "STAR") == 0)  r = x * y;
    else if (strcmp(op, "SLASH") == 0) {
        if (y == 0.0) return;          /* 5 / 0.0: fica como no fonte */
        r = x / y;
    } else {
        return;                        /* % << >> & | ^ ja deram erro de tipo */
    }
    if (e->type == TYPE_FLOAT) r = (float)r;
    if (!isfinite(r)) return;
    set_float(e, r);
}

/* aritmetica inteira, com a divisao truncada de C. Calcula em long long pra
   ver o overflow antes de dobrar */
static void fold_int_arith(Node *e, const char *op, long long a, long long b) {
    long long r;
    if (strcmp(op, "PLUS") == 0)       r = a + b;
    else if (strcmp(op, "MINUS") == 0) r = a - b;
    else if (strcmp(op, "STAR") == 0)  r = a * b;
    else if (strcmp(op, "SLASH") == 0 || strcmp(op, "MOD") == 0) {
        if (b == 0 || (a == INT_MIN && b == -1)) return;
        r = strcmp(op, "SLASH") == 0 ? a / b : a % b;
    }
    else if (strcmp(op, "EC") == 0)    r = a & b;
    else if (strcmp(op, "BINOR") == 0) r = a | b;
    else if (strcmp(op, "XOR") == 0)   r = a ^ b;
    else if (strcmp(op, "SHIFTL") == 0) {
        /* em C, shift fora de 0..31 e deslocar negativo pra esquerda sao UB */
        if (b < 0 || b >= 32 || a < 0) return;
        r = a << b;
    } else if (strcmp(op, "SHIFTR") == 0) {
        if (b < 0 || b >= 32) return;
        r = a >> b;                    /* aritmetico, como o gcc e o Rust */
    } else {
        return;
    }
    if (!fits_int(r)) return;
    set_int(e, r, 0);
}

static void fold_binary(Node *e) {
    Lit a, b;
    if (!literal_value(e->children[0], &a) || !literal_value(e->children[2], &b)) return;
    const char *op = e->children[1]->label;

    int r;
    if (compare(op, a, b, &r)) {
        set_int(e, r, 1);
    } else if (strcmp(op, "AND") == 0) {
        set_int(e, is_true(a) && is_true(b), 1);
    } else if (strcmp(op, "OR") == 0) {
        set_int(e, is_true(a) || is_true(b), 1);
    } else if (a.is_float || b.is_float) {
        fold_float_arith(e, op, as_double(a), as_double(b));
    } else {
        fold_int_arith(e, op, a.i, b.i);
    }
}

void fold_constants(Node *e) {
    if (!e || strcmp(e->label, "expr") != 0) return;
    /* erro de tipo (ja reportado), void ou tipo ainda desconhecido: nao mexe */
    if (!type_is_numeric(e->type)) return;

    if (e->n_children == 2) {
        fold_unary(e);
    } else if (e->n_children == 3 && strcmp(e->children[0]->label, "LPAREN") == 0) {
        /* (literal) -> literal: o no fica com a folha e a marca de dentro */
        Lit v;
        Node *inner = e->children[1];
        if (!literal_value(inner, &v)) return;
        /* a folha de dentro e liberada na troca: copia antes */
        const char *label = inner->children[0]->label;
        char *text = strdup(inner->children[0]->text);
        replace_with_leaf(e, label, text, inner->folded);
        free(text);
    } else if (e->n_children == 3) {
        fold_binary(e);
    }
}
