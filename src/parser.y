%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "types.h"

void yyerror(const char *s);
int yylex(void);

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

/* tipo da declaracao em andamento, setado pela acao intermediaria em
   `type_specifier` antes de reduzir declarator_list (statement e for_init) */
static DataType current_decl_type;

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

%union {
    DataType data_type;
    ValueData val;
    char *str;
}

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

%type <str> statement_list statement expr function_list function opt_param_list param_list param
%type <str> declarator declarator_list for_init for_cond for_incr
%type <str> opt_arg_list arg_list
%type <data_type> type_specifier

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

program:
    function_list {
        printf("%s", $1);
    }
;

function_list:
    function {
        $$ = $1;
    }
    | function_list function {
        char *buf = malloc(strlen($1) + strlen($2) + 2);
        sprintf(buf, "%s\n%s", $1, $2);
        $$ = buf;
    }
;

function:
    type_specifier IDENTIFIER LPAREN opt_param_list RPAREN LBRACE statement_list RBRACE {
        const char *ret_type = type_to_rust($1);
        char *buf = malloc(strlen($2.s_val) + strlen($4) + strlen(ret_type) + strlen($7) + 32);
        sprintf(buf, "fn %s(%s) -> %s {\n%s}\n", $2.s_val, $4, ret_type, $7);
        $$ = buf;
    }
;

opt_param_list:
    param_list {
        $$ = $1;
    }
    | /* vazio */ {
        char *buf = malloc(1);
        buf[0] = '\0';
        $$ = buf;
    }
;

param_list:
    param {
        $$ = $1;
    }
    | param_list ',' param {
        char *buf = malloc(strlen($1) + strlen($3) + 3);
        sprintf(buf, "%s, %s", $1, $3);
        $$ = buf;
    }
;

param:
    type_specifier IDENTIFIER {
        const char *t = type_to_rust($1);
        char *buf = malloc(strlen($2.s_val) + strlen(t) + 4);
        sprintf(buf, "%s: %s", $2.s_val, t);
        $$ = buf;
    }
;

type_specifier:
    TYPE_KW { $$ = $1; }
;

statement_list:
    statement { $$ = $1; }
    | statement_list statement {
        char *buf = malloc(strlen($1) + strlen($2) + 1);
        sprintf(buf, "%s%s", $1, $2);
        $$ = buf;
    }
;

statement:
    type_specifier { current_decl_type = $1; } declarator_list SEMICOLON {
        $$ = $3;
    }
    | IDENTIFIER ASSIGN expr SEMICOLON {
        char *buf = malloc(strlen($1.s_val) + strlen($3) + 32);
        sprintf(buf, "    %s = %s;\n", $1.s_val, $3);
        $$ = buf;
    }
    | RETURN expr SEMICOLON {
        char *buf = malloc(strlen($2) + 32);
        sprintf(buf, "    // return %s;\n", $2);
        $$ = buf;
    }
    | IF LPAREN expr RPAREN statement %prec IFX {
        $$ = format_str("    if %s {\n%s    }\n", $3, $5);
    }
    | IF LPAREN expr RPAREN statement ELSE statement {
        $$ = format_str("    if %s {\n%s    } else {\n%s    }\n", $3, $5, $7);
    }
    | WHILE LPAREN expr RPAREN statement {
        $$ = format_str("    while %s {\n%s    }\n", $3, $5);
    }
    | FOR LPAREN for_init SEMICOLON for_cond SEMICOLON for_incr RPAREN statement {
        $$ = format_str("    {\n%s        while %s {\n%s            %s\n        }\n    }\n",
                         $3, $5, $9, $7);
    }
    | LBRACE statement_list RBRACE {
        $$ = format_str("    {\n%s    }\n", $2);
    }
    | LBRACE RBRACE {
        $$ = strdup("    {}\n");
    }
    | BREAK SEMICOLON {
        $$ = strdup("    break;\n");
    }
    | CONTINUE SEMICOLON {
        $$ = strdup("    continue;\n");
    }
;

/* as tres secoes do for, cada uma opcional: for (init; cond; incr) */
for_init:
    /* vazio */ { $$ = strdup(""); }
    | type_specifier { current_decl_type = $1; } declarator_list { $$ = $3; }
    | IDENTIFIER ASSIGN expr { $$ = format_str("%s = %s;\n", $1.s_val, $3); }
;

for_cond:
    /* vazio */ { $$ = strdup("true"); }
    | expr { $$ = $1; }
;

for_incr:
    /* vazio */ { $$ = strdup(""); }
    | IDENTIFIER ASSIGN expr { $$ = format_str("%s = %s;", $1.s_val, $3); }
;

/* uma variavel dentro de uma declaracao, com ou sem inicializacao:
   `int x;` ou `int x = 5;` (usa current_decl_type pro tipo) */
declarator:
    IDENTIFIER {
        $$ = format_str("    let mut %s: %s;\n", $1.s_val, type_to_rust(current_decl_type));
    }
    | IDENTIFIER ASSIGN expr {
        $$ = format_str("    let mut %s: %s = %s;\n", $1.s_val, type_to_rust(current_decl_type), $3);
    }
;

declarator_list:
    declarator { $$ = $1; }
    | declarator_list ',' declarator {
        $$ = format_str("%s%s", $1, $3);
    }
;

expr:
    INT_NUMBER {
        char *buf = malloc(32);
        sprintf(buf, "%d", $1.i_val);
        $$ = buf;
    }
    | FLOAT_NUMBER {
        char *buf = malloc(32);
        sprintf(buf, "%f", $1.f_val);
        $$ = buf;
    }
    | IDENTIFIER {
        $$ = strdup($1.s_val);
    }
    | LPAREN expr RPAREN {
        if (is_wrapped($2)) {
            $$ = $2; /* ex. (a == b) ja vem entre parenteses do cmp_expr */
        } else {
            char *buf = malloc(strlen($2) + 3);
            sprintf(buf, "(%s)", $2);
            $$ = buf;
        }
    }
    | IDENTIFIER LPAREN opt_arg_list RPAREN {
        char *buf = malloc(strlen($1.s_val) + strlen($3) + 3);
        sprintf(buf, "%s(%s)", $1.s_val, $3);
        $$ = buf;
    }
    | expr PLUS expr    { $$ = binop_expr($1, "+", $3); }
    | expr MINUS expr   { $$ = binop_expr($1, "-", $3); }
    | expr STAR expr    { $$ = binop_expr($1, "*", $3); }
    | expr SLASH expr   { $$ = binop_expr($1, "/", $3); }
    | expr MOD expr     { $$ = binop_expr($1, "%", $3); }
    | expr XOR expr     { $$ = binop_expr($1, "^", $3); }
    | expr OR expr      { $$ = binop_expr($1, "||", $3); }
    | expr BINOR expr   { $$ = binop_expr($1, "|", $3); }
    | expr AND expr     { $$ = binop_expr($1, "&&", $3); }
    | expr EC expr      { $$ = binop_expr($1, "&", $3); }
    | expr SHIFTL expr  { $$ = binop_expr($1, "<<", $3); }
    | expr SHIFTR expr  { $$ = binop_expr($1, ">>", $3); }
    | expr COMP expr    { $$ = cmp_expr($1, "==", $3); }
    | expr NE expr      { $$ = cmp_expr($1, "!=", $3); }
    | expr LT expr      { $$ = cmp_expr($1, "<", $3); }
    | expr GT expr      { $$ = cmp_expr($1, ">", $3); }
    | expr LE expr      { $$ = cmp_expr($1, "<=", $3); }
    | expr GE expr      { $$ = cmp_expr($1, ">=", $3); }
    | NOT expr          { $$ = unop_expr("!", $2); }
    | MINUS expr %prec UMINUS { $$ = unop_expr("-", $2); }
;

/* argumentos de uma chamada: f(), f(a) ou f(a, b + 1, g(c)) */
opt_arg_list:
    arg_list { $$ = $1; }
    | /* vazio */ { $$ = strdup(""); }
;

arg_list:
    expr { $$ = $1; }
    | arg_list ',' expr {
        char *buf = malloc(strlen($1) + strlen($3) + 3);
        sprintf(buf, "%s, %s", $1, $3);
        $$ = buf;
    }
;

%%

extern int yylineno;
extern char *yytext;

void yyerror(const char *s) {
    fprintf(stderr, "Erro sintático na linha %d: %s (próximo a '%s')\n", yylineno, s, yytext);
}

const char *token_name(int tok) {
    return yytname[YYTRANSLATE(tok)];
}