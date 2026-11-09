#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tac.h"
#include "symtab.h"
#include "semantic.h"

static FILE *out;
static int ntemp, nlabel;
static DataType cur_ret;   /* tipo de retorno da funcao em andamento */

static char *xfmt(const char *fmt, ...) {
    va_list args, copy;
    va_start(args, fmt);
    va_copy(copy, args);
    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    char *buf = malloc(len + 1);
    vsnprintf(buf, len + 1, fmt, copy);
    va_end(copy);
    return buf;
}

/* uma instrucao, indentada */
static void emit(const char *fmt, ...) {
    va_list args;
    fputs("    ", out);
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
    fputc('\n', out);
}

static char *new_temp(void) { return xfmt("t%d", ntemp++); }
static int new_label(void) { return nlabel++; }
static void emit_label(int l) { fprintf(out, "L%d:\n", l); }

/* ---- variaveis visiveis ----
   O TAC guarda o seu proprio mapa nome -> (apelido, tipo): a tabela de
   simbolos do parser ja fechou os escopos locais. O tipo serve pras
   conversoes; o apelido separa variaveis de mesmo nome em escopos aninhados
   (`x` e `x.1`), que no TAC plano ficariam ambiguas */

typedef struct {
    char *name, *alias;
    DataType type;
} Var;

static Var *vars;
static int nvars, capvars;

static int scope_mark(void) { return nvars; }

static void scope_pop(int mark) {
    while (nvars > mark) {
        nvars--;
        free(vars[nvars].name);
        free(vars[nvars].alias);
    }
}

static Var *var_lookup(const char *name) {
    for (int i = nvars - 1; i >= 0; i--)
        if (strcmp(vars[i].name, name) == 0) return &vars[i];
    return NULL;
}

static void var_declare(const char *name, DataType type) {
    int shadowed = 0;
    for (int i = 0; i < nvars; i++)
        if (strcmp(vars[i].name, name) == 0) shadowed++;
    if (nvars == capvars) {
        capvars = capvars ? capvars * 2 : 16;
        vars = realloc(vars, capvars * sizeof(Var));
    }
    vars[nvars].name = strdup(name);
    vars[nvars].alias = shadowed ? xfmt("%s.%d", name, shadowed) : strdup(name);
    vars[nvars].type = type;
    nvars++;
}

/* ---- tipos ---- */

static DataType type_from_c(const char *name) {
    if (strcmp(name, "float") == 0) return TYPE_FLOAT;
    if (strcmp(name, "double") == 0) return TYPE_DOUBLE;
    if (strcmp(name, "char") == 0) return TYPE_CHAR;
    if (strcmp(name, "void") == 0) return TYPE_VOID;
    return TYPE_INT;
}

/* tipo declarado por um `type_specifier` */
static DataType spec_type(const Node *spec) {
    return type_from_c(spec->children[0]->text);
}

/* converte o operando `v` (tipo `from`) pra `to`, se forem tipos numericos
   diferentes: emite `t = (to) v` e devolve o temporario. Toma posse de `v` */
static char *conv(char *v, DataType from, DataType to) {
    if (from == to || !type_is_numeric(from) || !type_is_numeric(to)) return v;
    char *t = new_temp();
    emit("%s = (%s) %s", t, type_to_c(to), v);
    free(v);
    return t;
}

/* ---- expressoes ---- */

static char *gen_expr(const Node *e);

static const char *op_symbol(const char *tok) {
    static const char *table[][2] = {
        {"PLUS", "+"}, {"MINUS", "-"}, {"STAR", "*"}, {"SLASH", "/"}, {"MOD", "%"},
        {"XOR", "^"}, {"BINOR", "|"}, {"EC", "&"}, {"SHIFTL", "<<"}, {"SHIFTR", ">>"},
        {"COMP", "=="}, {"NE", "!="}, {"LT", "<"}, {"GT", ">"}, {"LE", "<="}, {"GE", ">="},
    };
    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++)
        if (strcmp(tok, table[i][0]) == 0) return table[i][1];
    return "?";
}

/* && e || com curto-circuito: o lado direito so roda se o esquerdo nao
   decidiu o resultado. O resultado e sempre 0 ou 1 */
static char *gen_logic(const Node *e, int is_and) {
    char *res = new_temp();
    int decided = new_label(), end = new_label();
    const char *jump = is_and ? "ifFalse" : "if";

    char *a = gen_expr(e->children[0]);
    emit("%s %s goto L%d", jump, a, decided);
    free(a);
    char *b = gen_expr(e->children[2]);
    emit("%s %s goto L%d", jump, b, decided);
    free(b);
    emit("%s = %d", res, is_and ? 1 : 0);
    emit("goto L%d", end);
    emit_label(decided);
    emit("%s = %d", res, is_and ? 0 : 1);
    emit_label(end);
    return res;
}

/* quantos argumentos tem um arg_list (aninha a esquerda) e quais sao */
static int args_collect(const Node *list, const Node **out) {
    if (list->n_children == 3) {
        int k = args_collect(list->children[0], out);
        out[k] = list->children[2];
        return k + 1;
    }
    out[0] = list->children[0];
    return 1;
}

static char *gen_call(const Node *e) {
    const char *name = e->children[0]->text;
    const Node *opt = e->children[2];
    const Node *list = opt->children[0];
    int n = strcmp(list->label, "arg_list") == 0 ? 1 : 0;
    for (const Node *l = list; n && l->n_children == 3; l = l->children[0]) n++;

    const Node **args = malloc((n + 1) * sizeof(Node *));
    char **ops = malloc((n + 1) * sizeof(char *));
    if (n) args_collect(list, args);
    const Symbol *f = symtab_lookup_func(name);

    /* os argumentos sao avaliados em ordem e so depois empilhados */
    for (int i = 0; i < n; i++) {
        ops[i] = gen_expr(args[i]);
        if (f && f->n_params == n) ops[i] = conv(ops[i], args[i]->type, f->param_types[i]);
    }
    for (int i = 0; i < n; i++) {
        emit("param %s", ops[i]);
        free(ops[i]);
    }
    free(ops);
    free(args);
    char *t = new_temp();
    emit("%s = call %s, %d", t, name, n);
    return t;
}

static char *gen_expr(const Node *e) {
    int n = e->n_children;

    if (n == 1) {
        const Node *leaf = e->children[0];
        if (strcmp(leaf->label, "IDENTIFIER") == 0) {
            const Var *v = var_lookup(leaf->text);
            return strdup(v ? v->alias : leaf->text);
        }
        return strdup(leaf->text);       /* INT_NUMBER ou FLOAT_NUMBER */
    }
    if (n == 4) return gen_call(e);
    if (n == 3 && strcmp(e->children[0]->label, "LPAREN") == 0)
        return gen_expr(e->children[1]);

    if (n == 2) {
        const Node *operand = e->children[1];
        int is_not = strcmp(e->children[0]->label, "NOT") == 0;
        char *v = gen_expr(operand);
        if (!is_not) v = conv(v, operand->type, e->type);    /* -c de char e int */
        char *t = new_temp();
        emit("%s = %s %s", t, is_not ? "!" : "-", v);
        free(v);
        return t;
    }

    /* binario: expr OP expr */
    const char *op = e->children[1]->label;
    if (strcmp(op, "AND") == 0) return gen_logic(e, 1);
    if (strcmp(op, "OR") == 0) return gen_logic(e, 0);

    const Node *l = e->children[0], *r = e->children[2];
    DataType common = TYPE_NONE;
    if (type_is_numeric(l->type) && type_is_numeric(r->type))
        common = type_promote(l->type, r->type);
    char *a = conv(gen_expr(l), l->type, common);
    char *b = conv(gen_expr(r), r->type, common);
    char *t = new_temp();
    emit("%s = %s %s %s", t, a, op_symbol(op), b);
    free(a);
    free(b);
    return t;
}

/* ---- comandos ---- */

/* laco em andamento: pra onde vao o break e o continue */
static int *brk_label, *cont_label;
static int loop_depth, loop_cap;

static void loop_push(int brk, int cont) {
    if (loop_depth == loop_cap) {
        loop_cap = loop_cap ? loop_cap * 2 : 8;
        brk_label = realloc(brk_label, loop_cap * sizeof(int));
        cont_label = realloc(cont_label, loop_cap * sizeof(int));
    }
    brk_label[loop_depth] = brk;
    cont_label[loop_depth] = cont;
    loop_depth++;
}

static void gen_statement(const Node *s);

/* `x = expr`: avalia, converte pro tipo de `x` e atribui */
static void gen_assign(const char *name, const Node *value) {
    const Var *v = var_lookup(name);
    char *r = gen_expr(value);
    if (v) r = conv(r, value->type, v->type);
    emit("%s = %s", v ? v->alias : name, r);
    free(r);
}

/* um declarador, `x` ou `x = expr`. Sem inicializador nao ha instrucao, so
   a variavel entra no escopo */
static void gen_declarator(const Node *d, DataType type) {
    const char *name = d->children[0]->text;
    if (d->n_children == 3) {
        /* o inicializador avalia antes de a variavel entrar no escopo */
        const Node *value = d->children[2];
        char *r = conv(gen_expr(value), value->type, type);
        var_declare(name, type);
        emit("%s = %s", var_lookup(name)->alias, r);
        free(r);
    } else {
        var_declare(name, type);
    }
}

/* declaradores aninhados a esquerda: `int a, b = 2, c;` */
static void gen_declarators(const Node *list, DataType type) {
    if (list->n_children == 3) {
        gen_declarators(list->children[0], type);
        gen_declarator(list->children[2], type);
    } else {
        gen_declarator(list->children[0], type);
    }
}

static void gen_statement_list(const Node *list) {
    if (list->n_children == 2) {
        gen_statement_list(list->children[0]);
        gen_statement(list->children[1]);
    } else {
        gen_statement(list->children[0]);
    }
}

/* for (init; cond; incr) body */
static void gen_for(const Node *s) {
    const Node *init = s->children[2], *cond = s->children[4];
    const Node *incr = s->children[6], *body = s->children[8];
    int mark = scope_mark();

    if (init->n_children == 2)
        gen_declarators(init->children[1], spec_type(init->children[0]));
    else if (init->n_children == 3)
        gen_assign(init->children[0]->text, init->children[2]);

    int top = new_label(), next = new_label(), end = new_label();
    emit_label(top);
    if (strcmp(cond->children[0]->label, "expr") == 0) {
        char *c = gen_expr(cond->children[0]);
        emit("ifFalse %s goto L%d", c, end);
        free(c);
    }
    loop_push(end, next);
    gen_statement(body);
    loop_depth--;
    emit_label(next);
    if (incr->n_children == 3) gen_assign(incr->children[0]->text, incr->children[2]);
    emit("goto L%d", top);
    emit_label(end);
    scope_pop(mark);
}

static void gen_statement(const Node *s) {
    const char *first = s->children[0]->label;

    if (strcmp(first, "type_specifier") == 0) {
        gen_declarators(s->children[1], spec_type(s->children[0]));
    } else if (strcmp(first, "IDENTIFIER") == 0) {
        gen_assign(s->children[0]->text, s->children[2]);
    } else if (strcmp(first, "RETURN") == 0) {
        const Node *value = s->children[1];
        char *r = conv(gen_expr(value), value->type, cur_ret);
        emit("return %s", r);
        free(r);
    } else if (strcmp(first, "IF") == 0) {
        int has_else = s->n_children == 7;
        int other = new_label(), end = has_else ? new_label() : other;
        char *c = gen_expr(s->children[2]);
        emit("ifFalse %s goto L%d", c, other);
        free(c);
        gen_statement(s->children[4]);
        if (has_else) {
            emit("goto L%d", end);
            emit_label(other);
            gen_statement(s->children[6]);
        }
        emit_label(end);
    } else if (strcmp(first, "WHILE") == 0) {
        int top = new_label(), end = new_label();
        emit_label(top);
        char *c = gen_expr(s->children[2]);
        emit("ifFalse %s goto L%d", c, end);
        free(c);
        loop_push(end, top);
        gen_statement(s->children[4]);
        loop_depth--;
        emit("goto L%d", top);
        emit_label(end);
    } else if (strcmp(first, "FOR") == 0) {
        gen_for(s);
    } else if (strcmp(first, "LBRACE") == 0) {
        if (s->n_children == 3) {
            int mark = scope_mark();
            gen_statement_list(s->children[1]);
            scope_pop(mark);
        }
    } else if (strcmp(first, "BREAK") == 0) {
        emit("goto L%d", brk_label[loop_depth - 1]);
    } else if (strcmp(first, "CONTINUE") == 0) {
        emit("goto L%d", cont_label[loop_depth - 1]);
    }
}

/* ---- funcoes ---- */

/* declara os parametros no escopo e acrescenta os nomes em *names */
static void gen_params(const Node *list, char **names) {
    if (strcmp(list->label, "param_list") == 0) {
        if (list->n_children == 3) {
            gen_params(list->children[0], names);
            gen_params(list->children[2], names);
        } else {
            gen_params(list->children[0], names);
        }
        return;
    }
    /* param: type_specifier IDENTIFIER */
    const char *name = list->children[1]->text;
    var_declare(name, spec_type(list->children[0]));
    char *joined = xfmt(**names ? "%s, %s" : "%s%s", *names, name);
    free(*names);
    *names = joined;
}

static void gen_function(const Node *f) {
    ntemp = nlabel = 0;
    cur_ret = spec_type(f->children[0]);
    int mark = scope_mark();

    char *names = strdup("");
    const Node *params = f->children[3];
    if (strcmp(params->children[0]->label, "param_list") == 0)
        gen_params(params->children[0], &names);
    fprintf(out, "func %s(%s):\n", f->children[1]->text, names);
    free(names);

    gen_statement_list(f->children[6]);
    fprintf(out, "endfunc\n");
    scope_pop(mark);
}

static void gen_function_list(const Node *list, int *first) {
    if (list->n_children == 2) gen_function_list(list->children[0], first);
    const Node *f = list->children[list->n_children - 1];
    if (!*first) fputc('\n', out);
    *first = 0;
    gen_function(f);
}

void tac_generate(const Node *root, FILE *dest) {
    out = dest;
    int first = 1;
    gen_function_list(root->children[0], &first);
    free(brk_label);
    free(cont_label);
    brk_label = cont_label = NULL;
    loop_depth = loop_cap = 0;
    scope_pop(0);
    free(vars);
    vars = NULL;
    capvars = 0;
}
