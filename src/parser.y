%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "types.h"
#include "options.h"
#include "ast.h"
#include "symtab.h"
#include "semantic.h"

void yyerror(const char *s);
int yylex(void);
extern int lexical_errors;

static char *binop_expr(char *lhs, const char *op, char *rhs) {
    char *buf = malloc(strlen(lhs) + strlen(op) + strlen(rhs) + 4);
    sprintf(buf, "%s %s %s", lhs, op, rhs);
    return buf;
}

/* monta strings de codegen maiores (if/while/for) sem contar tamanho na mao */
static char *format_str(const char *fmt, ...) {
    va_list args, args_copy;
    va_start(args, fmt);
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    char *buf = malloc(len + 1);
    vsnprintf(buf, len + 1, fmt, args_copy);
    va_end(args_copy);
    return buf;
}

/* cria um no `label` com o trecho de Rust `code` e anexa os filhos dados
   (lista terminada em NULL); o no passa a ser dono deles */
static Node *mk(const char *label, int line, char *code, ...) {
    Node *n = node_new(label, line);
    n->code = code;
    va_list args;
    va_start(args, code);
    for (Node *c; (c = va_arg(args, Node *)) != NULL; )
        node_add_child(n, c);
    va_end(args);
    return n;
}

/* folha de um token de texto fixo (pontuacao, operador, palavra reservada):
   so o nome do token, sem lexema. Os tokens com lexema proprio (IDENTIFIER,
   TYPE_KW, numeros) usam node_leaf direto */
static Node *tok(const char *label, int line) {
    return node_leaf(label, NULL, line);
}

/* no de expressao binaria `expr OP expr`: o operador vira folha do token
   (PLUS, STAR...) entre os dois operandos. `gen` e binop_expr ou cmp_expr */
static Node *binop_node(Node *lhs, const char *op_tok, const char *op, int op_line, Node *rhs,
                        char *(*gen)(char *, const char *, char *)) {
    return mk("expr", lhs->line, gen(lhs->code, op, rhs->code),
              lhs, tok(op_tok, op_line), rhs, NULL);
}

/* tipo da declaracao em andamento, setado pela acao intermediaria em
   `type_specifier` antes de reduzir declarator_list (statement e for_init) */
static DataType current_decl_type;

/* assinatura da funcao em andamento: o tipo de retorno e guardado logo
   depois do LPAREN (antes que os params troquem o current_decl_type), e cada
   `param` acrescenta o seu tipo. A funcao entra na tabela depois do RPAREN */
static DataType current_func_ret;
static DataType *current_func_params;
static int current_func_n, current_func_cap;

/* uso de `name` como variavel (em expressao ou como alvo de atribuicao):
   tem que haver uma variavel ou parametro visivel com esse nome */
static void check_var_use(const char *name, int line) {
    Symbol *sym = symtab_lookup(name);
    if (!sym)
        semantic_error(line, "variável '%s' não declarada", name);
    else if (sym->kind == SYM_FUNC)
        semantic_error(line, "'%s' é uma função, não uma variável", name);
}

/* quantos argumentos tem um opt_arg_list: o vazio tem so o filho ε, e o
   arg_list aninha a esquerda (arg_list ',' expr) */
static int count_args(const Node *opt_arg_list) {
    const Node *list = opt_arg_list->children[0];
    if (strcmp(list->label, "arg_list") != 0) return 0;
    int n = 1;
    while (list->n_children == 3) {
        n++;
        list = list->children[0];
    }
    return n;
}

/* declaracao de variavel: redeclarar no mesmo escopo e erro, mas esconder
   um nome de um escopo externo (sombreamento) e valido */
static void declare_var(const char *name, int line) {
    if (symtab_insert(name, SYM_VAR, current_decl_type, line) != 0)
        semantic_error(line, "variável '%s' já declarada neste escopo (linha %d)",
                       name, symtab_lookup_current(name)->line);
}

static void func_param_add(DataType t) {
    if (current_func_n == current_func_cap) {
        current_func_cap = current_func_cap ? current_func_cap * 2 : 4;
        current_func_params = realloc(current_func_params, current_func_cap * sizeof(DataType));
    }
    current_func_params[current_func_n++] = t;
}

/* comparacoes saem sempre entre parenteses: em Rust elas tem precedencia
   menor que & ^ | (em C e o contrario) e nao podem ser encadeadas, entao
   `a & b == c` precisa virar `a & (b == c)` pra manter o agrupamento de C */
static char *cmp_expr(char *lhs, const char *op, char *rhs) {
    char *buf = malloc(strlen(lhs) + strlen(op) + strlen(rhs) + 6);
    sprintf(buf, "(%s %s %s)", lhs, op, rhs);
    return buf;
}

static char *unop_expr(const char *op, char *operand) {
    /* `- -a` nao pode colar em `--a`, que parece o decremento de C */
    const char *sep = (op[0] == '-' && operand[0] == '-') ? " " : "";
    char *buf = malloc(strlen(op) + strlen(sep) + strlen(operand) + 1);
    sprintf(buf, "%s%s%s", op, sep, operand);
    return buf;
}

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
%}

/* o Node da %union precisa estar visivel em parser.tab.h (lexer e main) */
%code requires {
#include "ast.h"
}

/* no erro sintatico o bison descarta os nos que ja estavam na pilha: sem
   isto eles vazariam (a arvore parcial nunca chega em parse_root) */
%destructor { node_free($$); } <node>

%union {
    DataType data_type;
    ValueData val;
    Node *node;
}

/* @N.first_line da a linha de cada simbolo (o lexer preenche yylloc em
   YY_USER_ACTION); yylineno na acao pode estar adiantado pelo lookahead */
%locations

/* expoe yytname pra token_name() (usado pelo modo --tokens do main.c) */
%token-table

%token <val> IDENTIFIER INT_NUMBER FLOAT_NUMBER
%token <data_type> TYPE_KW
%token RETURN ASSIGN SEMICOLON LBRACE RBRACE LPAREN RPAREN
%token PLUS MINUS STAR MOD XOR OR BINOR AND EC SHIFTL SHIFTR COMP
%token IF ELSE WHILE FOR BREAK CONTINUE

/* tokens do lexico completo (issue #2) - a gramatica ainda nao usa todos */
%token <val> CHAR_LITERAL STRING_LITERAL
%token STRUCT IF ELSE WHILE FOR BREAK CONTINUE
%token NE LT GT LE GE NOT SLASH DOT ARROW
%token INC DEC ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN
%token AND_ASSIGN OR_ASSIGN XOR_ASSIGN SHL_ASSIGN SHR_ASSIGN

%type <node> statement_list statement expr function_list function opt_param_list param_list param
%type <node> declarator declarator_list for_init for_cond for_incr
%type <node> opt_arg_list arg_list type_specifier

/* precedencia e associatividade de C, da menor pra maior */
%left OR
%left AND
%left BINOR
%left XOR
%left EC
%left COMP NE
%left LT GT LE GE
%left SHIFTL SHIFTR
%left PLUS MINUS
%left STAR SLASH MOD
%precedence NOT UMINUS

/* dangling-else: favorece o shift do ELSE, amarrando ao if mais próximo */
%precedence IFX
%precedence ELSE

%%

/* o escopo global e criado pelo symtab_init() do main, antes do parse, e
   nao fecha (scope_exit no global nao faz nada) */
program:
    function_list {
        parse_root = mk("program", @1.first_line, NULL, $1, NULL);
        /* com o programa inteiro lido, todas as funcoes ja estao na tabela */
        semantic_check_calls();
        /* o Rust so sai quando nenhuma flag de analise esta ligada
           (--tree, --dot, --symtab): a saida e a analise, nao o codigo.
           Programa com erro lexico ou semantico e invalido e tambem nao
           gera Rust */
        if (emit_code && lexical_errors == 0 && semantic_errors == 0) printf("%s", $1->code);
    }
;

function_list:
    function {
        $$ = mk("function_list", @1.first_line, strdup($1->code), $1, NULL);
    }
    | function_list function {
        char *buf = malloc(strlen($1->code) + strlen($2->code) + 2);
        sprintf(buf, "%s\n%s", $1->code, $2->code);
        $$ = mk("function_list", @1.first_line, buf, $1, $2, NULL);
    }
;

/* parametros e corpo dividem o mesmo escopo (como em C): abre logo depois
   do LPAREN e fecha depois do RBRACE. A funcao entra no escopo global logo
   depois do RPAREN, ja com a assinatura e antes do corpo (pra recursao).
   As acoes do meio ocupam o $4 e o $7, entao opt_param_list e o $5 e
   statement_list o $9 */
function:
    type_specifier IDENTIFIER LPAREN {
        scope_enter();
        current_func_ret = current_decl_type;
        current_func_n = 0;
    }
    opt_param_list RPAREN {
        if (symtab_insert_func($2.s_val, current_func_ret, current_func_n,
                               current_func_params, @2.first_line) != 0) {
            semantic_error(@2.first_line, "função '%s' já definida (linha %d)",
                           $2.s_val, symtab_lookup_func($2.s_val)->line);
        }
    }
    LBRACE statement_list RBRACE {
        scope_exit();
        const char *ret_type = $1->code;
        char *buf = malloc(strlen($2.s_val) + strlen($5->code) + strlen(ret_type) + strlen($9->code) + 32);
        sprintf(buf, "fn %s(%s) -> %s {\n%s}\n", $2.s_val, $5->code, ret_type, $9->code);
        $$ = mk("function", @1.first_line, buf,
                $1, node_leaf("IDENTIFIER", $2.s_val, @2.first_line), tok("LPAREN", @3.first_line),
                $5, tok("RPAREN", @6.first_line), tok("LBRACE", @8.first_line),
                $9, tok("RBRACE", @10.first_line), NULL);
    }
;

opt_param_list:
    param_list {
        $$ = mk("opt_param_list", @1.first_line, strdup($1->code), $1, NULL);
    }
    | /* vazio */ {
        char *buf = malloc(1);
        buf[0] = '\0';
        $$ = mk("opt_param_list", @$.first_line, buf, node_leaf("ε", NULL, @$.first_line), NULL);
    }
;

param_list:
    param {
        $$ = mk("param_list", @1.first_line, strdup($1->code), $1, NULL);
    }
    | param_list ',' param {
        char *buf = malloc(strlen($1->code) + strlen($3->code) + 3);
        sprintf(buf, "%s, %s", $1->code, $3->code);
        $$ = mk("param_list", @1.first_line, buf, $1, tok("','", @2.first_line), $3, NULL);
    }
;

param:
    type_specifier IDENTIFIER {
        /* o type_specifier acabou de ser reduzido: current_decl_type e o tipo dele */
        if (symtab_insert($2.s_val, SYM_PARAM, current_decl_type, @2.first_line) != 0) {
            semantic_error(@2.first_line, "parâmetro '%s' repetido (linha %d)",
                           $2.s_val, symtab_lookup_current($2.s_val)->line);
        }
        /* o repetido conta na aridade mesmo assim: f(int a, int a) recebe 2 */
        func_param_add(current_decl_type);
        const char *t = $1->code;
        char *buf = malloc(strlen($2.s_val) + strlen(t) + 4);
        sprintf(buf, "%s: %s", $2.s_val, t);
        $$ = mk("param", @1.first_line, buf, $1, node_leaf("IDENTIFIER", $2.s_val, @2.first_line), NULL);
    }
;

type_specifier:
    TYPE_KW {
        /* reduzido antes de declarator_list, entao ja vale pra declaracao */
        current_decl_type = $1;
        $$ = mk("type_specifier", @1.first_line, strdup(type_to_rust($1)),
                node_leaf("TYPE_KW", type_to_c($1), @1.first_line), NULL);
    }
;

statement_list:
    statement {
        $$ = mk("statement_list", @1.first_line, strdup($1->code), $1, NULL);
    }
    | statement_list statement {
        char *buf = malloc(strlen($1->code) + strlen($2->code) + 1);
        sprintf(buf, "%s%s", $1->code, $2->code);
        $$ = mk("statement_list", @1.first_line, buf, $1, $2, NULL);
    }
;

statement:
    type_specifier declarator_list SEMICOLON {
        /* o Rust da declaracao e o dos declarators (um `let mut` por variavel) */
        $$ = mk("statement", @1.first_line, strdup($2->code),
                $1, $2, tok("SEMICOLON", @3.first_line), NULL);
    }
    | IDENTIFIER ASSIGN expr SEMICOLON {
        check_var_use($1.s_val, @1.first_line);
        char *buf = malloc(strlen($1.s_val) + strlen($3->code) + 32);
        sprintf(buf, "    %s = %s;\n", $1.s_val, $3->code);
        $$ = mk("statement", @1.first_line, buf,
                node_leaf("IDENTIFIER", $1.s_val, @1.first_line), tok("ASSIGN", @2.first_line),
                $3, tok("SEMICOLON", @4.first_line), NULL);
    }
    | RETURN expr SEMICOLON {
        char *buf = malloc(strlen($2->code) + 32);
        sprintf(buf, "    return %s;\n", $2->code);
        $$ = mk("statement", @1.first_line, buf,
                tok("RETURN", @1.first_line), $2, tok("SEMICOLON", @3.first_line), NULL);
    }
    | IF LPAREN expr RPAREN statement %prec IFX {
        $$ = mk("statement", @1.first_line,
                format_str("    if %s {\n%s    }\n", $3->code, $5->code),
                tok("IF", @1.first_line), tok("LPAREN", @2.first_line), $3,
                tok("RPAREN", @4.first_line), $5, NULL);
    }
    | IF LPAREN expr RPAREN statement ELSE statement {
        /* o ELSE fica no mesmo no do IF mais proximo (dangling-else) */
        $$ = mk("statement", @1.first_line,
                format_str("    if %s {\n%s    } else {\n%s    }\n", $3->code, $5->code, $7->code),
                tok("IF", @1.first_line), tok("LPAREN", @2.first_line), $3,
                tok("RPAREN", @4.first_line), $5, tok("ELSE", @6.first_line), $7, NULL);
    }
    | WHILE LPAREN expr RPAREN statement {
        $$ = mk("statement", @1.first_line,
                format_str("    while %s {\n%s    }\n", $3->code, $5->code),
                tok("WHILE", @1.first_line), tok("LPAREN", @2.first_line), $3,
                tok("RPAREN", @4.first_line), $5, NULL);
    }
    /* o for tem escopo proprio, aberto antes do for_init, pra que o `i` de
       `for (int i = 0; ...)` nao vaze pro resto da funcao. A acao do meio
       ocupa o $3 */
    | FOR LPAREN { scope_enter(); } for_init SEMICOLON for_cond SEMICOLON for_incr RPAREN statement {
        scope_exit();
        $$ = mk("statement", @1.first_line,
                format_str("    {\n%s        while %s {\n%s            %s\n        }\n    }\n",
                           $4->code, $6->code, $10->code, $8->code),
                tok("FOR", @1.first_line), tok("LPAREN", @2.first_line), $4,
                tok("SEMICOLON", @5.first_line), $6, tok("SEMICOLON", @7.first_line), $8,
                tok("RPAREN", @9.first_line), $10, NULL);
    }
    /* cada bloco abre um escopo; a acao do meio ocupa o $2 */
    | LBRACE { scope_enter(); } statement_list RBRACE {
        scope_exit();
        $$ = mk("statement", @1.first_line, format_str("    {\n%s    }\n", $3->code),
                tok("LBRACE", @1.first_line), $3, tok("RBRACE", @4.first_line), NULL);
    }
    | LBRACE RBRACE {
        /* bloco vazio tambem conta como escopo (sem simbolos) */
        scope_enter();
        scope_exit();
        $$ = mk("statement", @1.first_line, strdup("    {}\n"),
                tok("LBRACE", @1.first_line), tok("RBRACE", @2.first_line), NULL);
    }
    | BREAK SEMICOLON {
        $$ = mk("statement", @1.first_line, strdup("    break;\n"),
                tok("BREAK", @1.first_line), tok("SEMICOLON", @2.first_line), NULL);
    }
    | CONTINUE SEMICOLON {
        $$ = mk("statement", @1.first_line, strdup("    continue;\n"),
                tok("CONTINUE", @1.first_line), tok("SEMICOLON", @2.first_line), NULL);
    }
;

/* as tres secoes do for, cada uma opcional: for (init; cond; incr).
   Secao vazia vira o no da secao com um filho ε */
for_init:
    /* vazio */ {
        $$ = mk("for_init", @$.first_line, strdup(""), node_leaf("ε", NULL, @$.first_line), NULL);
    }
    | type_specifier declarator_list {
        $$ = mk("for_init", @1.first_line, strdup($2->code), $1, $2, NULL);
    }
    | IDENTIFIER ASSIGN expr {
        check_var_use($1.s_val, @1.first_line);
        $$ = mk("for_init", @1.first_line, format_str("%s = %s;\n", $1.s_val, $3->code),
                node_leaf("IDENTIFIER", $1.s_val, @1.first_line), tok("ASSIGN", @2.first_line), $3, NULL);
    }
;

for_cond:
    /* vazio */ {
        $$ = mk("for_cond", @$.first_line, strdup("true"), node_leaf("ε", NULL, @$.first_line), NULL);
    }
    | expr { $$ = mk("for_cond", @1.first_line, strdup($1->code), $1, NULL); }
;

for_incr:
    /* vazio */ {
        $$ = mk("for_incr", @$.first_line, strdup(""), node_leaf("ε", NULL, @$.first_line), NULL);
    }
    | IDENTIFIER ASSIGN expr {
        check_var_use($1.s_val, @1.first_line);
        $$ = mk("for_incr", @1.first_line, format_str("%s = %s;", $1.s_val, $3->code),
                node_leaf("IDENTIFIER", $1.s_val, @1.first_line), tok("ASSIGN", @2.first_line), $3, NULL);
    }
;

/* uma variavel dentro de uma declaracao, com ou sem inicializacao:
   `int x;` ou `int x = 5;` (usa current_decl_type pro tipo) */
declarator:
    IDENTIFIER {
        declare_var($1.s_val, @1.first_line);
        $$ = mk("declarator", @1.first_line,
                format_str("    let mut %s: %s;\n", $1.s_val, type_to_rust(current_decl_type)),
                node_leaf("IDENTIFIER", $1.s_val, @1.first_line), NULL);
    }
    | IDENTIFIER ASSIGN expr {
        declare_var($1.s_val, @1.first_line);
        $$ = mk("declarator", @1.first_line,
                format_str("    let mut %s: %s = %s;\n", $1.s_val, type_to_rust(current_decl_type), $3->code),
                node_leaf("IDENTIFIER", $1.s_val, @1.first_line), tok("ASSIGN", @2.first_line), $3, NULL);
    }
;

/* recursiva a esquerda: `int a, b, c;` fica aninhada, sem achatar */
declarator_list:
    declarator {
        $$ = mk("declarator_list", @1.first_line, strdup($1->code), $1, NULL);
    }
    | declarator_list ',' declarator {
        $$ = mk("declarator_list", @1.first_line, format_str("%s%s", $1->code, $3->code),
                $1, tok("','", @2.first_line), $3, NULL);
    }
;

expr:
    INT_NUMBER {
        char *buf = malloc(32);
        sprintf(buf, "%d", $1.i_val);
        $$ = mk("expr", @1.first_line, buf, node_leaf("INT_NUMBER", $1.s_val, @1.first_line), NULL);
    }
    | FLOAT_NUMBER {
        char *buf = malloc(32);
        sprintf(buf, "%f", $1.f_val);
        $$ = mk("expr", @1.first_line, buf, node_leaf("FLOAT_NUMBER", $1.s_val, @1.first_line), NULL);
    }
    | IDENTIFIER {
        check_var_use($1.s_val, @1.first_line);
        $$ = mk("expr", @1.first_line, strdup($1.s_val), node_leaf("IDENTIFIER", $1.s_val, @1.first_line), NULL);
    }
    | LPAREN expr RPAREN {
        char *buf;
        if (is_wrapped($2->code)) {
            buf = strdup($2->code); /* ex. (a == b) ja vem entre parenteses do cmp_expr */
        } else {
            buf = malloc(strlen($2->code) + 3);
            sprintf(buf, "(%s)", $2->code);
        }
        /* a arvore guarda os parenteses como estao no fonte, independente do codegen */
        $$ = mk("expr", @1.first_line, buf,
                tok("LPAREN", @1.first_line), $2, tok("RPAREN", @3.first_line), NULL);
    }
    | IDENTIFIER LPAREN opt_arg_list RPAREN {
        /* checada so no fim do programa: a funcao pode vir depois */
        semantic_call($1.s_val, count_args($3), @1.first_line);
        char *buf = malloc(strlen($1.s_val) + strlen($3->code) + 3);
        sprintf(buf, "%s(%s)", $1.s_val, $3->code);
        $$ = mk("expr", @1.first_line, buf,
                node_leaf("IDENTIFIER", $1.s_val, @1.first_line), tok("LPAREN", @2.first_line),
                $3, tok("RPAREN", @4.first_line), NULL);
    }
    | expr PLUS expr    { $$ = binop_node($1, "PLUS", "+", @2.first_line, $3, binop_expr); }
    | expr MINUS expr   { $$ = binop_node($1, "MINUS", "-", @2.first_line, $3, binop_expr); }
    | expr STAR expr    { $$ = binop_node($1, "STAR", "*", @2.first_line, $3, binop_expr); }
    | expr SLASH expr   { $$ = binop_node($1, "SLASH", "/", @2.first_line, $3, binop_expr); }
    | expr MOD expr     { $$ = binop_node($1, "MOD", "%", @2.first_line, $3, binop_expr); }
    | expr XOR expr     { $$ = binop_node($1, "XOR", "^", @2.first_line, $3, binop_expr); }
    | expr OR expr      { $$ = binop_node($1, "OR", "||", @2.first_line, $3, binop_expr); }
    | expr BINOR expr   { $$ = binop_node($1, "BINOR", "|", @2.first_line, $3, binop_expr); }
    | expr AND expr     { $$ = binop_node($1, "AND", "&&", @2.first_line, $3, binop_expr); }
    | expr EC expr      { $$ = binop_node($1, "EC", "&", @2.first_line, $3, binop_expr); }
    | expr SHIFTL expr  { $$ = binop_node($1, "SHIFTL", "<<", @2.first_line, $3, binop_expr); }
    | expr SHIFTR expr  { $$ = binop_node($1, "SHIFTR", ">>", @2.first_line, $3, binop_expr); }
    | expr COMP expr    { $$ = binop_node($1, "COMP", "==", @2.first_line, $3, cmp_expr); }
    | expr NE expr      { $$ = binop_node($1, "NE", "!=", @2.first_line, $3, cmp_expr); }
    | expr LT expr      { $$ = binop_node($1, "LT", "<", @2.first_line, $3, cmp_expr); }
    | expr GT expr      { $$ = binop_node($1, "GT", ">", @2.first_line, $3, cmp_expr); }
    | expr LE expr      { $$ = binop_node($1, "LE", "<=", @2.first_line, $3, cmp_expr); }
    | expr GE expr      { $$ = binop_node($1, "GE", ">=", @2.first_line, $3, cmp_expr); }
    | NOT expr {
        $$ = mk("expr", @1.first_line, unop_expr("!", $2->code), tok("NOT", @1.first_line), $2, NULL);
    }
    | MINUS expr %prec UMINUS {
        $$ = mk("expr", @1.first_line, unop_expr("-", $2->code), tok("MINUS", @1.first_line), $2, NULL);
    }
;

/* argumentos de uma chamada: f(), f(a) ou f(a, b + 1, g(c)) */
opt_arg_list:
    arg_list { $$ = mk("opt_arg_list", @1.first_line, strdup($1->code), $1, NULL); }
    | /* vazio */ {
        $$ = mk("opt_arg_list", @$.first_line, strdup(""), node_leaf("ε", NULL, @$.first_line), NULL);
    }
;

arg_list:
    expr { $$ = mk("arg_list", @1.first_line, strdup($1->code), $1, NULL); }
    | arg_list ',' expr {
        char *buf = malloc(strlen($1->code) + strlen($3->code) + 3);
        sprintf(buf, "%s, %s", $1->code, $3->code);
        $$ = mk("arg_list", @1.first_line, buf, $1, tok("','", @2.first_line), $3, NULL);
    }
;

%%

extern int yylineno;
extern char *yytext;

void yyerror(const char *s) {
    /* no EOF o yytext vem vazio: "proximo a ''" nao diz nada */
    if (yytext[0] == '\0')
        fprintf(stderr, "Erro sintático na linha %d: %s (no fim do arquivo)\n", yylineno, s);
    else
        fprintf(stderr, "Erro sintático na linha %d: %s (próximo a '%s')\n", yylineno, s, yytext);
}

/* libera o que o parser guarda entre as regras (chamada pelo main no fim) */
void parser_cleanup(void) {
    free(current_func_params);
    current_func_params = NULL;
    current_func_n = current_func_cap = 0;
}

const char *token_name(int tok) {
    return yytname[YYTRANSLATE(tok)];
}
