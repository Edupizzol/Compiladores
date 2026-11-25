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

/* 1 se a subarvore pode ter efeito colateral. A unica construcao do nosso
   subconjunto com efeito colateral e a chamada de funcao, que tem a forma
   `expr -> IDENTIFIER LPAREN opt_arg_list RPAREN` (4 filhos) */
static int has_side_effect(const Node *n) {
    if (!n) return 0;
    if (strcmp(n->label, "expr") == 0 && n->n_children == 4 &&
        strcmp(n->children[1]->label, "LPAREN") == 0)
        return 1;
    for (int i = 0; i < n->n_children; i++)
        if (has_side_effect(n->children[i])) return 1;
    return 0;
}

/* substitui o conteudo de `e` pelo subtree `keep` (um dos operandos). O
   no `e` fica com o tipo dele, entao o codegen poe o cast quando o tipo
   difere do de keep (ex. `x` int e `(x + 0.0)` double: o Rust sai
   `(x as f64)`). Os outros filhos sao liberados. `keep` passa a ser
   consumido (filhos adotados por `e`, o no de `keep` em si e liberado) */
static void adopt_subtree(Node *e, Node *keep) {
    for (int i = 0; i < e->n_children; i++)
        if (e->children[i] != keep) node_free(e->children[i]);
    free(e->children);
    e->children = keep->children;
    e->n_children = keep->n_children;
    e->cap = keep->cap;
    e->folded = keep->folded;

    free(keep->text);
    free(keep);
}

/* 1 se `e` e um literal com valor `want` (inteiro ou float: `0` casa com
   `0` e com `0.0`, o que vale pros neutros `x+0`, `x*1` etc em qualquer
   tipo numerico) */
static int lit_eq(const Node *e, double want) {
    Lit v;
    if (!literal_value(e, &v)) return 0;
    return as_double(v) == want;
}

/* igual, mas exige literal inteiro. Serve pros casos que so valem em int:
   absorvente (`x*0`, `x&0`), reducao de forca (`x*2`), e os operadores
   bit a bit, onde `0` so pode ser int */
static int int_lit_eq(const Node *e, long long want) {
    Lit v;
    if (!literal_value(e, &v) || v.is_float) return 0;
    return v.i == want;
}

/* troca `x * 2` por `x << 1`: mantem o operando `keep`, substitui o `STAR`
   por `SHIFTL` e o literal `2` por `1` (o Rust sai do no SHIFTL). So inteiro.
   Avaliacao unica do operando, entao vale mesmo com efeito colateral */
static void strength_mul2(Node *e, int keep_idx, int lit_idx) {
    Node *keep = e->children[keep_idx];
    node_free(e->children[lit_idx]);
    node_free(e->children[1]);

    Node *one_expr = node_new("expr", e->line);
    node_add_child(one_expr, node_leaf("INT_NUMBER", "1", e->line));
    one_expr->type = TYPE_INT;

    e->children[0] = keep;
    e->children[1] = node_leaf("SHIFTL", NULL, e->line);
    e->children[2] = one_expr;
    e->n_children = 3;
}

static void simplify_binary(Node *e) {
    Node *lhs = e->children[0], *rhs = e->children[2];
    const char *op = e->children[1]->label;

    if (strcmp(op, "PLUS") == 0) {
        if (lit_eq(rhs, 0)) { adopt_subtree(e, lhs); return; }
        if (lit_eq(lhs, 0)) { adopt_subtree(e, rhs); return; }
    } else if (strcmp(op, "MINUS") == 0) {
        /* so `x - 0` -> `x`; `0 - x` vira `-x` e precisaria de outro no */
        if (lit_eq(rhs, 0)) { adopt_subtree(e, lhs); return; }
    } else if (strcmp(op, "STAR") == 0) {
        if (lit_eq(rhs, 1)) { adopt_subtree(e, lhs); return; }
        if (lit_eq(lhs, 1)) { adopt_subtree(e, rhs); return; }
        /* absorvente e reducao de forca so em inteiro: `x * 0.0` pode dar
           NaN quando `x` e Inf, entao nao vale como identidade */
        if (e->type == TYPE_INT) {
            if (int_lit_eq(rhs, 0) && !has_side_effect(lhs)) { set_int(e, 0, 0); return; }
            if (int_lit_eq(lhs, 0) && !has_side_effect(rhs)) { set_int(e, 0, 0); return; }
            if (int_lit_eq(rhs, 2)) { strength_mul2(e, 0, 2); return; }
            if (int_lit_eq(lhs, 2)) { strength_mul2(e, 2, 0); return; }
        }
    } else if (strcmp(op, "SLASH") == 0) {
        /* so `x / 1` -> `x`; `x / x` nao (float NaN, int div por zero) */
        if (lit_eq(rhs, 1)) { adopt_subtree(e, lhs); return; }
    } else if (strcmp(op, "BINOR") == 0 || strcmp(op, "XOR") == 0) {
        if (int_lit_eq(rhs, 0)) { adopt_subtree(e, lhs); return; }
        if (int_lit_eq(lhs, 0)) { adopt_subtree(e, rhs); return; }
    } else if (strcmp(op, "EC") == 0) {
        if (int_lit_eq(rhs, 0) && !has_side_effect(lhs)) { set_int(e, 0, 0); return; }
        if (int_lit_eq(lhs, 0) && !has_side_effect(rhs)) { set_int(e, 0, 0); return; }
    } else if (strcmp(op, "SHIFTL") == 0 || strcmp(op, "SHIFTR") == 0) {
        /* so `x << 0`, `x >> 0`; `0 << x` nao e neutro */
        if (int_lit_eq(rhs, 0)) { adopt_subtree(e, lhs); return; }
    }
}

void simplify_algebraic(Node *e) {
    if (!e || strcmp(e->label, "expr") != 0) return;
    if (!type_is_numeric(e->type)) return;
    if (e->n_children == 3 && strcmp(e->children[0]->label, "LPAREN") == 0) {
        /* `(simple)` -> `simple`: parenteses em volta de identificador (ou
           de literal que ja foi dobrado em folha unica) nao mudam o
           significado e evitam `(x) * 1` virar `(x)` em vez de `x`. Call
           (`(f())`, 4 filhos) fica, pra nao virar `f()` desacompanhado */
        Node *inner = e->children[1];
        if (inner->n_children == 1) adopt_subtree(e, inner);
        return;
    }
    /* so binario `expr OP expr`: unario nao se enquadra nas identidades */
    if (e->n_children != 3) return;
    if (strcmp(e->children[0]->label, "expr") != 0) return;
    simplify_binary(e);
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

/* s adota os filhos de keep (um filho de s); os outros filhos saem */
static void adopt_statement(Node *s, Node *keep) {
    for (int i = 0; i < s->n_children; i++)
        if (s->children[i] != keep) node_free(s->children[i]);
    free(s->children);
    s->children = keep->children;
    s->n_children = keep->n_children;
    s->cap = keep->cap;
    s->folded = keep->folded;   /* o ramo que fica pode ja ter sido apagado */
    free(keep->text);
    free(keep);
}

/* o statement some: vira um bloco vazio na arvore, marcado com FOLD_DEAD pra
   o codegen nao imprimir nada (um `{ }` do fonte continua saindo `{}`) */
static void make_empty(Node *s) {
    for (int i = 0; i < s->n_children; i++)
        node_free(s->children[i]);
    s->n_children = 0;
    node_add_child(s, node_leaf("LBRACE", NULL, s->line));
    node_add_child(s, node_leaf("RBRACE", NULL, s->line));
    s->folded = FOLD_DEAD;
}

static void dead_if(Node *s) {
    Lit v;
    if (!literal_value(s->children[2], &v)) return;
    int has_else = s->n_children == 7;
    Node *then_st = s->children[4];
    Node *else_st = has_else ? s->children[6] : NULL;
    Node *keep = is_true(v) ? then_st : else_st;

    if (!keep) { make_empty(s); return; }
    /* declaracao solta como ramo: desembrulhar tiraria o escopo dela */
    if (strcmp(keep->children[0]->label, "type_specifier") == 0) return;
    adopt_statement(s, keep);
}

static int cond_is_false(const Node *cond) {
    Lit v;
    return literal_value(cond, &v) && !is_true(v);
}

static void dead_while(Node *s) {
    if (cond_is_false(s->children[2])) make_empty(s);
}

/* for com condicao falsa: o corpo e o incremento somem, o init fica
   (pode ter efeito colateral) dentro de um bloco, pra manter o escopo */
static void dead_for(Node *s) {
    Node *init = s->children[2], *cond = s->children[4];
    if (strcmp(cond->children[0]->label, "expr") != 0) return;
    if (!cond_is_false(cond->children[0])) return;
    if (init->n_children == 1) { make_empty(s); return; }

    Node *st = node_new("statement", init->line);
    if (init->n_children == 2) {
        node_add_child(st, init->children[0]);
        node_add_child(st, init->children[1]);
    } else {
        node_add_child(st, init->children[0]);
        node_add_child(st, init->children[1]);
        node_add_child(st, init->children[2]);
    }
    node_add_child(st, node_leaf("SEMICOLON", NULL, init->line));
    init->n_children = 0;

    Node *list = node_new("statement_list", init->line);
    node_add_child(list, st);

    for (int i = 0; i < s->n_children; i++)
        node_free(s->children[i]);
    s->n_children = 0;
    node_add_child(s, node_leaf("LBRACE", NULL, s->line));
    node_add_child(s, list);
    node_add_child(s, node_leaf("RBRACE", NULL, s->line));
}

void eliminate_dead_code(Node *s) {
    if (!s || strcmp(s->label, "statement") != 0) return;
    const char *first = s->children[0]->label;
    if (strcmp(first, "IF") == 0) dead_if(s);
    else if (strcmp(first, "WHILE") == 0) dead_while(s);
    else if (strcmp(first, "FOR") == 0) dead_for(s);
}

static int is_jump(const Node *st) {
    const char *f = st->children[0]->label;
    return strcmp(f, "RETURN") == 0 || strcmp(f, "BREAK") == 0 || strcmp(f, "CONTINUE") == 0;
}

/* a lista termina em return/break/continue? um comando ja apagado
   (FOLD_DEAD) nao conta: olha o anterior */
static int ends_with_jump(const Node *list) {
    const Node *last = list->children[list->n_children - 1];
    if (is_jump(last)) return 1;
    if (last->folded == FOLD_DEAD && list->n_children == 2)
        return ends_with_jump(list->children[0]);
    return 0;
}

void drop_after_jump(const Node *list, Node *s) {
    if (ends_with_jump(list)) make_empty(s);
}