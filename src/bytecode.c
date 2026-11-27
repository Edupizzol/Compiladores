#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bytecode.h"
#include "symtab.h"
#include "semantic.h"

static FILE *out;
static int nlabel;
static DataType cur_ret;

static void emit(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
    fputc('\n', out);
}

static int new_label(void) { return nlabel++; }

/* ---- variaveis visiveis: mesmo esquema do TAC (x, x.1, x.2) ---- */

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
    if (shadowed) {
        vars[nvars].alias = malloc(strlen(name) + 16);
        sprintf(vars[nvars].alias, "%s.%d", name, shadowed);
    } else {
        vars[nvars].alias = strdup(name);
    }
    vars[nvars].type = type;
    nvars++;
}

/* ---- tipos ---- */

static DataType spec_type(const Node *spec) {
    const char *name = spec->children[0]->text;
    if (strcmp(name, "float") == 0) return TYPE_FLOAT;
    if (strcmp(name, "double") == 0) return TYPE_DOUBLE;
    if (strcmp(name, "char") == 0) return TYPE_CHAR;
    if (strcmp(name, "void") == 0) return TYPE_VOID;
    return TYPE_INT;
}

/* int e char sao o mesmo tipo na pilha; so float e double mudam a conversao */
static int rank(DataType t) { return t == TYPE_DOUBLE ? 2 : t == TYPE_FLOAT ? 1 : 0; }

/* o valor no topo da pilha e de tipo `from` e tem que virar `to` */
static void conv(DataType from, DataType to) {
    if (!type_is_numeric(from) || !type_is_numeric(to)) return;
    static const char *name[3][3] = {
        {NULL, "I2F", "I2D"},
        {"F2I", NULL, "F2D"},
        {"D2I", "D2F", NULL},
    };
    const char *n = name[rank(from)][rank(to)];
    if (n) emit("%s", n);
}

/* ---- expressoes: o resultado fica no topo da pilha ---- */

static void gen_expr(const Node *e);

static const char *op_name(const char *tok) {
    static const char *table[][2] = {
        {"PLUS", "ADD"}, {"MINUS", "SUB"}, {"STAR", "MUL"}, {"SLASH", "DIV"}, {"MOD", "MOD"},
        {"XOR", "BIT_XOR"}, {"BINOR", "BIT_OR"}, {"EC", "BIT_AND"},
        {"SHIFTL", "SHL"}, {"SHIFTR", "SHR"},
        {"COMP", "EQ"}, {"NE", "NE"}, {"LT", "LT"}, {"GT", "GT"}, {"LE", "LE"}, {"GE", "GE"},
    };
    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++)
        if (strcmp(tok, table[i][0]) == 0) return table[i][1];
    return "?";
}

/* && e || com curto-circuito. O resultado e sempre 0 ou 1. Como so ha
   JUMP_IF_FALSE, o || nega o valor antes de testar */
static void gen_logic(const Node *e, int is_and) {
    int decided = new_label(), end = new_label();
    for (int i = 0; i < 3; i += 2) {
        gen_expr(e->children[i]);
        if (!is_and) emit("NOT");
        emit("JUMP_IF_FALSE L%d", decided);
    }
    emit("LOAD_CONST %d", is_and ? 1 : 0);
    emit("JUMP L%d", end);
    emit("LABEL L%d", decided);
    emit("LOAD_CONST %d", is_and ? 0 : 1);
    emit("LABEL L%d", end);
}

static int args_collect(const Node *list, const Node **out_args) {
    if (list->n_children == 3) {
        int k = args_collect(list->children[0], out_args);
        out_args[k] = list->children[2];
        return k + 1;
    }
    out_args[0] = list->children[0];
    return 1;
}

static void gen_call(const Node *e) {
    const char *name = e->children[0]->text;
    const Node *list = e->children[2]->children[0];
    int n = strcmp(list->label, "arg_list") == 0 ? 1 : 0;
    for (const Node *l = list; n && l->n_children == 3; l = l->children[0]) n++;

    const Node **args = malloc((n + 1) * sizeof(Node *));
    if (n) args_collect(list, args);
    const Symbol *f = symtab_lookup_func(name);

    /* os argumentos sao empilhados em ordem, ja no tipo do parametro */
    for (int i = 0; i < n; i++) {
        gen_expr(args[i]);
        if (f && f->n_params == n) conv(args[i]->type, f->param_types[i]);
    }
    free(args);
    emit("CALL %s %d", name, n);
}

static void gen_expr(const Node *e) {
    int n = e->n_children;

    if (n == 1) {
        const Node *leaf = e->children[0];
        if (strcmp(leaf->label, "IDENTIFIER") == 0) {
            const Var *v = var_lookup(leaf->text);
            emit("LOAD %s", v ? v->alias : leaf->text);
        } else {
            emit("LOAD_CONST %s", leaf->text);
        }
        return;
    }
    if (n == 4) { gen_call(e); return; }
    if (n == 3 && strcmp(e->children[0]->label, "LPAREN") == 0) {
        gen_expr(e->children[1]);
        return;
    }

    if (n == 2) {
        const Node *operand = e->children[1];
        int is_not = strcmp(e->children[0]->label, "NOT") == 0;
        gen_expr(operand);
        if (is_not) {
            emit("NOT");
        } else {
            conv(operand->type, e->type);
            emit("NEG");
        }
        return;
    }

    const char *op = e->children[1]->label;
    if (strcmp(op, "AND") == 0) { gen_logic(e, 1); return; }
    if (strcmp(op, "OR") == 0) { gen_logic(e, 0); return; }

    const Node *l = e->children[0], *r = e->children[2];
    DataType common = TYPE_NONE;
    if (type_is_numeric(l->type) && type_is_numeric(r->type))
        common = type_promote(l->type, r->type);
    gen_expr(l);
    conv(l->type, common);
    gen_expr(r);
    conv(r->type, common);
    emit("%s", op_name(op));
}

/* ---- comandos ---- */

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

static void gen_assign(const char *name, const Node *value) {
    const Var *v = var_lookup(name);
    gen_expr(value);
    if (v) conv(value->type, v->type);
    emit("STORE %s", v ? v->alias : name);
}

/* sem inicializador nao ha instrucao, so a variavel entra no escopo */
static void gen_declarator(const Node *d, DataType type) {
    const char *name = d->children[0]->text;
    if (d->n_children == 3) {
        /* o inicializador avalia antes de a variavel entrar no escopo */
        const Node *value = d->children[2];
        gen_expr(value);
        conv(value->type, type);
        var_declare(name, type);
        emit("STORE %s", var_lookup(name)->alias);
    } else {
        var_declare(name, type);
    }
}

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

static void gen_for(const Node *s) {
    const Node *init = s->children[2], *cond = s->children[4];
    const Node *incr = s->children[6], *body = s->children[8];
    int mark = scope_mark();

    if (init->n_children == 2)
        gen_declarators(init->children[1], spec_type(init->children[0]));
    else if (init->n_children == 3)
        gen_assign(init->children[0]->text, init->children[2]);

    int top = new_label(), next = new_label(), end = new_label();
    emit("LABEL L%d", top);
    if (strcmp(cond->children[0]->label, "expr") == 0) {
        gen_expr(cond->children[0]);
        emit("JUMP_IF_FALSE L%d", end);
    }
    loop_push(end, next);
    gen_statement(body);
    loop_depth--;
    emit("LABEL L%d", next);
    if (incr->n_children == 3) gen_assign(incr->children[0]->text, incr->children[2]);
    emit("JUMP L%d", top);
    emit("LABEL L%d", end);
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
        gen_expr(value);
        conv(value->type, cur_ret);
        emit("RET");
    } else if (strcmp(first, "IF") == 0) {
        int has_else = s->n_children == 7;
        int other = new_label(), end = has_else ? new_label() : other;
        gen_expr(s->children[2]);
        emit("JUMP_IF_FALSE L%d", other);
        gen_statement(s->children[4]);
        if (has_else) {
            emit("JUMP L%d", end);
            emit("LABEL L%d", other);
            gen_statement(s->children[6]);
        }
        emit("LABEL L%d", end);
    } else if (strcmp(first, "WHILE") == 0) {
        int top = new_label(), end = new_label();
        emit("LABEL L%d", top);
        gen_expr(s->children[2]);
        emit("JUMP_IF_FALSE L%d", end);
        loop_push(end, top);
        gen_statement(s->children[4]);
        loop_depth--;
        emit("JUMP L%d", top);
        emit("LABEL L%d", end);
    } else if (strcmp(first, "FOR") == 0) {
        gen_for(s);
    } else if (strcmp(first, "LBRACE") == 0) {
        if (s->n_children == 3) {
            int mark = scope_mark();
            gen_statement_list(s->children[1]);
            scope_pop(mark);
        }
    } else if (strcmp(first, "BREAK") == 0) {
        emit("JUMP L%d", brk_label[loop_depth - 1]);
    } else if (strcmp(first, "CONTINUE") == 0) {
        emit("JUMP L%d", cont_label[loop_depth - 1]);
    }
}

/* ---- funcoes ---- */

/* acumula os parametros (nome e tipo) na ordem em que aparecem */
static void params_collect(const Node *list, const char **names, DataType *types, int *n) {
    if (strcmp(list->label, "param_list") == 0) {
        if (list->n_children == 3) {
            params_collect(list->children[0], names, types, n);
            params_collect(list->children[2], names, types, n);
        } else {
            params_collect(list->children[0], names, types, n);
        }
        return;
    }
    names[*n] = list->children[1]->text;
    types[*n] = spec_type(list->children[0]);
    (*n)++;
}

static int params_count(const Node *list) {
    if (strcmp(list->label, "param_list") != 0) return 1;
    if (list->n_children == 3)
        return params_count(list->children[0]) + params_count(list->children[2]);
    return params_count(list->children[0]);
}

static void gen_function(const Node *f) {
    cur_ret = spec_type(f->children[0]);
    int mark = scope_mark();

    emit("LABEL %s", f->children[1]->text);

    /* o chamador empilhou os argumentos em ordem, entao o ultimo esta no
       topo: os parametros saem da pilha de tras pra frente */
    const Node *params = f->children[3];
    if (strcmp(params->children[0]->label, "param_list") == 0) {
        int total = params_count(params->children[0]);
        const char **names = malloc(total * sizeof(char *));
        DataType *types = malloc(total * sizeof(DataType));
        int n = 0;
        params_collect(params->children[0], names, types, &n);
        for (int i = 0; i < n; i++) var_declare(names[i], types[i]);
        for (int i = n - 1; i >= 0; i--) emit("STORE %s", var_lookup(names[i])->alias);
        free(names);
        free(types);
    }

    gen_statement_list(f->children[6]);
    if (cur_ret == TYPE_VOID) emit("RET");
    scope_pop(mark);
}

static void gen_function_list(const Node *list) {
    if (list->n_children == 2) gen_function_list(list->children[0]);
    gen_function(list->children[list->n_children - 1]);
}

void bytecode_generate(const Node *root, FILE *dest) {
    out = dest;
    nlabel = 0;
    gen_function_list(root->children[0]);
    free(brk_label);
    free(cont_label);
    brk_label = cont_label = NULL;
    loop_depth = loop_cap = 0;
    scope_pop(0);
    free(vars);
    vars = NULL;
    capvars = 0;
}