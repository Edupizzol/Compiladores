#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen.h"
#include "symtab.h"
#include "semantic.h"

/* monta strings de tamanho qualquer (malloc novo) */
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

static int label_is(const Node *n, const char *label) {
    return strcmp(n->label, label) == 0;
}

/* ---- variaveis visiveis ----
   So servem pro tipo do alvo de uma atribuicao (`x = 2.5` com `x` int vira
   `x = (2.500000 as i32)`): a tabela de simbolos ja fechou os escopos
   locais, entao o codegen refaz os escopos como o parser fazia (funcao,
   bloco e for). Os nomes apontam pro lexema das folhas, que vive ate o fim */

typedef struct {
    const char *name;
    DataType type;
} Var;

static Var *vars;
static int nvars, capvars;

static int scope_mark(void) { return nvars; }
static void scope_pop(int mark) { nvars = mark; }

static void var_declare(const char *name, DataType type) {
    if (nvars == capvars) {
        capvars = capvars ? capvars * 2 : 16;
        vars = realloc(vars, capvars * sizeof(Var));
    }
    vars[nvars].name = name;
    vars[nvars].type = type;
    nvars++;
}

static DataType var_type(const char *name) {
    for (int i = nvars - 1; i >= 0; i--)
        if (strcmp(vars[i].name, name) == 0) return vars[i].type;
    return TYPE_ERROR;
}

/* ---- funcoes ja definidas ----
   Uma funcao entra na tabela depois do `)` dela, entao uma chamada so
   conhece a assinatura se a funcao vem antes no fonte (ou e a propria
   funcao, na recursao). Chamada a funcao definida mais adiante tem tipo
   desconhecido e os argumentos vao sem cast */

static const char **funcs;
static int nfuncs, capfuncs;

static void func_define(const char *name) {
    if (nfuncs == capfuncs) {
        capfuncs = capfuncs ? capfuncs * 2 : 8;
        funcs = realloc(funcs, capfuncs * sizeof(char *));
    }
    funcs[nfuncs++] = name;
}

static const Symbol *func_known(const char *name) {
    for (int i = 0; i < nfuncs; i++)
        if (strcmp(funcs[i], name) == 0) return symtab_lookup_func(name);
    return NULL;
}

static DataType cur_ret;   /* tipo de retorno da funcao em andamento */

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

/* ---- expressoes ----
   Cada expressao devolve o Rust e o tipo dela. O tipo e o da arvore, menos
   quando depende de uma chamada a funcao definida mais adiante: ai e
   TYPE_UNKNOWN, como era no parse, e nao entra cast (o Rust era gerado nas
   acoes, antes do semantic_resolve_types, e a saida tem que continuar a
   mesma) */

typedef struct {
    char *code;
    DataType type;
} Expr;

static Expr gen_expr(const Node *e);

/* 1 se a expressao inteira ja esta entre um unico par de parenteses,
   ex. "(a == b)" -> 1, "(a) + (b)" -> 0 */
static int is_wrapped(const char *s) {
    size_t len = strlen(s);
    if (len < 2 || s[0] != '(' || s[len - 1] != ')') return 0;
    int depth = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '(') depth++;
        else if (s[i] == ')') depth--;
        if (depth == 0 && i < len - 1) return 0;
    }
    return 1;
}

/* 1 se o codigo e um identificador ou numero simples (nao precisa de
   parenteses antes de um `as`) */
static int is_simple(const char *s) {
    for (; *s; s++)
        if (!((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'z') ||
              (*s >= 'A' && *s <= 'Z') || *s == '_' || *s == '.'))
            return 0;
    return 1;
}

/* o cast de Rust equivalente a conversao implicita de C. char nao faz cast
   direto de/para float em Rust, entao passa por i32 (ou u8) */
static char *cast_code(const char *code, DataType from, DataType to) {
    const char *inner = (is_simple(code) || is_wrapped(code)) ? "%s" : "(%s)";
    char fmt[64];
    if (to == TYPE_CHAR)
        snprintf(fmt, sizeof fmt, "(%s as u8 as char)", inner);
    else if (from == TYPE_CHAR && to != TYPE_INT)
        snprintf(fmt, sizeof fmt, "(%s as i32 as %%s)", inner);
    else
        snprintf(fmt, sizeof fmt, "(%s as %%s)", inner);
    return xfmt(fmt, code, type_to_rust(to));
}

static int is_float_literal(const Node *n) {
    return n->n_children == 1 && label_is(n->children[0], "FLOAT_NUMBER");
}

/* codigo da expressao `x` (no `n`) convertido pro tipo `to`. So converte
   entre tipos numericos diferentes; o literal float nao precisa (o do Rust
   se ajusta a f32/f64). Toma posse de x.code */
static char *coerce(Expr x, const Node *n, DataType to) {
    DataType from = x.type;
    if (!type_is_numeric(from) || !type_is_numeric(to) || from == to)
        return x.code;
    if (is_float_literal(n) && (to == TYPE_FLOAT || to == TYPE_DOUBLE))
        return x.code;
    char *c = cast_code(x.code, from, to);
    free(x.code);
    return c;
}

/* gera a expressao `n` ja convertida pro tipo `to` */
static char *gen_coerced(const Node *n, DataType to) {
    return coerce(gen_expr(n), n, to);
}

/* gera a expressao `n` sem conversao */
static char *gen_plain(const Node *n) {
    return gen_expr(n).code;
}

static char *unop_code(const char *op, char *operand) {
    /* `- -a` nao pode colar em `--a`, que parece o decremento de C */
    const char *sep = (op[0] == '-' && operand[0] == '-') ? " " : "";
    char *buf = xfmt("%s%s%s", op, sep, operand);
    free(operand);
    return buf;
}

/* tipos de operador: aritmetico, so inteiro (% << >> & | ^), comparacao e
   logico (&& ||) */
enum { OPK_ARITH, OPK_INT, OPK_CMP, OPK_LOGIC };

static const struct {
    const char *tok, *rust;
    int kind;
} ops[] = {
    {"PLUS", "+", OPK_ARITH}, {"MINUS", "-", OPK_ARITH}, {"STAR", "*", OPK_ARITH},
    {"SLASH", "/", OPK_ARITH}, {"MOD", "%", OPK_INT}, {"XOR", "^", OPK_INT},
    {"OR", "||", OPK_LOGIC}, {"BINOR", "|", OPK_INT}, {"AND", "&&", OPK_LOGIC},
    {"EC", "&", OPK_INT}, {"SHIFTL", "<<", OPK_INT}, {"SHIFTR", ">>", OPK_INT},
    {"COMP", "==", OPK_CMP}, {"NE", "!=", OPK_CMP}, {"LT", "<", OPK_CMP},
    {"GT", ">", OPK_CMP}, {"LE", "<=", OPK_CMP}, {"GE", ">=", OPK_CMP},
};

/* `expr OP expr`: com as conversoes aritmeticas de C, os operandos ganham
   cast quando os tipos diferem (comparacao e logico dao int). Comparacao sai
   entre parenteses: em Rust ela tem precedencia menor que & ^ | (em C e o
   contrario) e nao pode ser encadeada, entao `a & b == c` vira
   `a & (b == c)` pra manter o agrupamento de C */
static Expr gen_binop(const Node *e) {
    const char *tok = e->children[1]->label, *op = "?";
    int kind = OPK_ARITH;
    for (size_t i = 0; i < sizeof ops / sizeof ops[0]; i++)
        if (strcmp(tok, ops[i].tok) == 0) {
            op = ops[i].rust;
            kind = ops[i].kind;
        }

    Expr l = gen_expr(e->children[0]), r = gen_expr(e->children[2]);
    DataType res, operand = TYPE_NONE;
    if (l.type == TYPE_UNKNOWN || r.type == TYPE_UNKNOWN) {
        res = TYPE_UNKNOWN;
    } else {
        operand = type_promote(l.type, r.type);
        res = (kind == OPK_ARITH || kind == OPK_INT) ? operand : TYPE_INT;
        if (kind == OPK_LOGIC) operand = TYPE_NONE;
    }

    char *lc = coerce(l, e->children[0], operand), *rc = coerce(r, e->children[2], operand);
    Expr out = {xfmt(kind == OPK_CMP ? "(%s %s %s)" : "%s %s %s", lc, op, rc), res};
    free(lc);
    free(rc);
    return out;
}

/* argumentos de um opt_arg_list (arg_list aninha a esquerda: o ultimo e o
   filho da direita). Devolve quantos sao e o vetor em *out (malloc) */
static int collect_args(const Node *opt_arg_list, const Node ***out) {
    const Node *list = opt_arg_list->children[0];
    int n = 0;
    for (const Node *l = list; label_is(l, "arg_list"); l = l->children[0]) n++;
    const Node **args = malloc((n + 1) * sizeof(Node *));
    for (int i = n - 1; i >= 0; i--) {
        args[i] = list->children[list->n_children - 1];
        list = list->children[0];
    }
    *out = args;
    return n;
}

/* `f(a, b)`: com a funcao ja definida, cada argumento vira o tipo do
   parametro (com cast se difere) e o resultado tem o tipo de retorno */
static Expr gen_call(const Node *e) {
    const char *name = e->children[0]->text;
    const Node **args;
    int n = collect_args(e->children[2], &args);
    const Symbol *f = func_known(name);

    char *list = strdup("");
    for (int i = 0; i < n; i++) {
        char *a = (f && f->n_params == n) ? gen_coerced(args[i], f->param_types[i])
                                          : gen_plain(args[i]);
        char *joined = xfmt(i ? "%s, %s" : "%s%s", list, a);
        free(list);
        free(a);
        list = joined;
    }
    free(args);
    Expr out = {xfmt("%s(%s)", name, list), f ? f->type : TYPE_UNKNOWN};
    free(list);
    return out;
}

/* literal: o do fonte sai como o parser o leu (`%d`, e `%f` no float); o
   criado pelo -O sai como o lexema, e a comparacao dobrada como bool */
static char *gen_literal(const Node *e) {
    const Node *leaf = e->children[0];
    if (e->folded == FOLD_BOOL) return strdup(atoi(leaf->text) ? "true" : "false");
    if (e->folded == FOLD_VALUE) return strdup(leaf->text);
    if (label_is(leaf, "INT_NUMBER")) return xfmt("%d", atoi(leaf->text));
    return xfmt("%f", atof(leaf->text));
}

static Expr gen_expr_inner(const Node *e);

/* o Rust da expressao, com o tipo da arvore. O conteudo tem o tipo dele (a
   variavel, o operador, a funcao); sem -O e sempre o da arvore. Com -O, a
   simplificacao (#50) pode trocar `x + 0.0` pelo `x` e deixar o no com o
   tipo double e o conteudo int: ai entra o cast, `(x as f64)` */
static Expr gen_expr(const Node *e) {
    Expr x = gen_expr_inner(e);
    if (type_is_numeric(x.type) && type_is_numeric(e->type) && x.type != e->type) {
        x.code = coerce(x, e, e->type);
        x.type = e->type;
    }
    return x;
}

static Expr gen_expr_inner(const Node *e) {
    int n = e->n_children;

    if (n == 1) {
        const Node *leaf = e->children[0];
        if (label_is(leaf, "IDENTIFIER")) {
            DataType t = var_type(leaf->text);
            return (Expr){strdup(leaf->text), t == TYPE_ERROR ? e->type : t};
        }
        return (Expr){gen_literal(e), e->type};
    }
    if (n == 4) return gen_call(e);
    if (n == 3 && label_is(e->children[0], "LPAREN")) {
        Expr in = gen_expr(e->children[1]);
        /* ex. (a == b) ja vem entre parenteses da comparacao */
        if (!is_wrapped(in.code)) {
            char *c = xfmt("(%s)", in.code);
            free(in.code);
            in.code = c;
        }
        return in;
    }
    if (n == 2) {
        const Node *operand = e->children[1];
        Expr in = gen_expr(operand);
        if (label_is(e->children[0], "NOT")) {
            DataType t = in.type == TYPE_UNKNOWN ? TYPE_UNKNOWN : TYPE_INT;
            return (Expr){unop_code("!", in.code), t};
        }
        /* `-` promove char pra int */
        DataType t = in.type == TYPE_UNKNOWN ? TYPE_UNKNOWN : type_promote(in.type, TYPE_INT);
        return (Expr){unop_code("-", coerce(in, operand, t)), t};
    }
    return gen_binop(e);
}

/* ---- comandos ---- */

static char *gen_stmt(const Node *s);

static char *concat(char *a, char *b, const char *sep) {
    char *r = xfmt("%s%s%s", a, sep, b);
    free(a);
    free(b);
    return r;
}

/* `IDENTIFIER ASSIGN expr`: o valor vira o tipo da variavel */
static char *gen_assign(const Node *n, const char *fmt) {
    const char *name = n->children[0]->text;
    char *val = gen_coerced(n->children[2], var_type(name));
    char *buf = xfmt(fmt, name, val);
    free(val);
    return buf;
}

/* `int x;` ou `int x = 5;`: o valor e gerado antes de declarar o `x`, como
   no parser (em `int x = x` o `x` da direita e o de fora) */
static char *gen_declarator(const Node *d, DataType t) {
    const char *name = d->children[0]->text;
    char *buf;
    if (d->n_children == 1) {
        buf = xfmt("    let mut %s: %s;\n", name, type_to_rust(t));
    } else {
        char *val = gen_coerced(d->children[2], t);
        buf = xfmt("    let mut %s: %s = %s;\n", name, type_to_rust(t), val);
        free(val);
    }
    var_declare(name, t);
    return buf;
}

/* recursiva a esquerda: `int a, b, c;`, um `let mut` por variavel */
static char *gen_declarator_list(const Node *l, DataType t) {
    if (l->n_children == 1) return gen_declarator(l->children[0], t);
    char *left = gen_declarator_list(l->children[0], t);
    return concat(left, gen_declarator(l->children[2], t), "");
}

static char *gen_stmt_list(const Node *l) {
    if (l->n_children == 1) return gen_stmt(l->children[0]);
    char *left = gen_stmt_list(l->children[0]);
    return concat(left, gen_stmt(l->children[1]), "");
}

/* as tres secoes do for; a vazia tem so o filho ε */
static char *gen_for_init(const Node *n) {
    const Node *first = n->children[0];
    if (label_is(first, "ε")) return strdup("");
    if (label_is(first, "type_specifier")) return gen_declarator_list(n->children[1], spec_type(first));
    return gen_assign(n, "%s = %s;\n");
}

static char *gen_for_cond(const Node *n) {
    if (label_is(n->children[0], "ε")) return strdup("true");
    return gen_plain(n->children[0]);
}

static char *gen_for_incr(const Node *n) {
    if (label_is(n->children[0], "ε")) return strdup("");
    return gen_assign(n, "%s = %s;");
}

/* `for (init; cond; incr) corpo` vira um bloco com o init e um while com o
   passo no fim. O escopo do for vale pras tres secoes e pro corpo; o incr e
   gerado antes do corpo, na ordem em que o parser lia */
static char *gen_for(const Node *s) {
    int mark = scope_mark();
    char *init = gen_for_init(s->children[2]);
    char *cond = gen_for_cond(s->children[4]);
    char *incr = gen_for_incr(s->children[6]);
    char *body = gen_stmt(s->children[8]);
    scope_pop(mark);
    char *buf = xfmt("    {\n%s        while %s {\n%s            %s\n        }\n    }\n",
                     init, cond, body, incr);
    free(init);
    free(cond);
    free(incr);
    free(body);
    return buf;
}

/* `if`/`while`: `cond` e o corpo, com o `else` opcional */
static char *gen_if_while(const Node *s, const char *kw) {
    char *cond = gen_plain(s->children[2]);
    char *then = gen_stmt(s->children[4]);
    char *buf;
    if (s->n_children == 7) {
        char *els = gen_stmt(s->children[6]);
        buf = xfmt("    %s %s {\n%s    } else {\n%s    }\n", kw, cond, then, els);
        free(els);
    } else {
        buf = xfmt("    %s %s {\n%s    }\n", kw, cond, then);
    }
    free(cond);
    free(then);
    return buf;
}

static char *gen_stmt(const Node *s) {
    const Node *first = s->children[0];
    const char *k = first->label;

    if (strcmp(k, "type_specifier") == 0)
        return gen_declarator_list(s->children[1], spec_type(first));
    if (strcmp(k, "IDENTIFIER") == 0)
        return gen_assign(s, "    %s = %s;\n");
    if (strcmp(k, "RETURN") == 0) {
        /* em funcao void o valor ja deu erro; sem cast pra () */
        char *val = cur_ret == TYPE_VOID ? gen_plain(s->children[1])
                                         : gen_coerced(s->children[1], cur_ret);
        char *buf = xfmt("    return %s;\n", val);
        free(val);
        return buf;
    }
    if (strcmp(k, "IF") == 0) return gen_if_while(s, "if");
    if (strcmp(k, "WHILE") == 0) return gen_if_while(s, "while");
    if (strcmp(k, "FOR") == 0) return gen_for(s);
    if (strcmp(k, "LBRACE") == 0) {
        if (s->n_children == 2) return strdup("    {}\n");
        int mark = scope_mark();
        char *body = gen_stmt_list(s->children[1]);
        scope_pop(mark);
        char *buf = xfmt("    {\n%s    }\n", body);
        free(body);
        return buf;
    }
    if (strcmp(k, "BREAK") == 0) return strdup("    break;\n");
    return strdup("    continue;\n");
}

/* ---- funcoes ---- */

/* `int a, double b` -> `a: i32, b: f64`; os parametros entram no escopo da
   funcao */
static char *gen_params(const Node *l) {
    if (label_is(l, "param")) {
        DataType t = spec_type(l->children[0]);
        const char *name = l->children[1]->text;
        var_declare(name, t);
        return xfmt("%s: %s", name, type_to_rust(t));
    }
    if (l->n_children == 1) return gen_params(l->children[0]);
    char *left = gen_params(l->children[0]);
    return concat(left, gen_params(l->children[2]), ", ");
}

/* filhos: type_specifier IDENTIFIER ( opt_param_list ) { statement_list } */
static char *gen_function(const Node *f) {
    DataType ret = spec_type(f->children[0]);
    const char *name = f->children[1]->text;
    const Node *opt_params = f->children[3];

    int mark = scope_mark();
    char *params = label_is(opt_params->children[0], "ε") ? strdup("")
                                                          : gen_params(opt_params->children[0]);
    /* a funcao ja vale dentro do proprio corpo (recursao) */
    func_define(name);
    cur_ret = ret;
    char *body = gen_stmt_list(f->children[6]);
    scope_pop(mark);

    char *buf = xfmt("fn %s(%s) -> %s {\n%s}\n", name, params, type_to_rust(ret), body);
    free(params);
    free(body);
    return buf;
}

static char *gen_function_list(const Node *l) {
    if (l->n_children == 1) return gen_function(l->children[0]);
    char *left = gen_function_list(l->children[0]);
    return concat(left, gen_function(l->children[1]), "\n");
}

void codegen_rust(const Node *root, FILE *out) {
    if (!root) return;
    char *code = gen_function_list(root->children[0]);
    fputs(code, out);
    free(code);

    free(vars);
    vars = NULL;
    nvars = capvars = 0;
    free(funcs);
    funcs = NULL;
    nfuncs = capfuncs = 0;
}
