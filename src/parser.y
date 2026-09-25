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
%}

%union {
    DataType data_type;
    ValueData val;
    char *str;
}

%token <val> IDENTIFIER INT_NUMBER FLOAT_NUMBER
%token <data_type> TYPE_KW
%token RETURN ASSIGN SEMICOLON LBRACE RBRACE LPAREN RPAREN
%token PLUS MINUS STAR MOD XOR OR BINOR AND EC SHIFTL SHIFTR COMP
%token IF ELSE WHILE FOR BREAK CONTINUE

%type <str> statement_list statement expr function_list function opt_param_list param_list param
%type <str> declarator declarator_list
%type <data_type> type_specifier

%left OR
%left AND
%left BINOR
%left XOR
%left EC
%left SHIFTL SHIFTR
%left PLUS MINUS
%left STAR MOD

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
        char *buf = malloc(strlen($2) + 3);
        sprintf(buf, "(%s)", $2);
        $$ = buf;
    }
    | expr PLUS expr    { $$ = binop_expr($1, "+", $3); }
    | expr MINUS expr   { $$ = binop_expr($1, "-", $3); }
    | expr STAR expr    { $$ = binop_expr($1, "*", $3); }
    | expr MOD expr     { $$ = binop_expr($1, "%", $3); }
    | expr XOR expr     { $$ = binop_expr($1, "^", $3); }
    | expr OR expr      { $$ = binop_expr($1, "||", $3); }
    | expr BINOR expr   { $$ = binop_expr($1, "|", $3); }
    | expr AND expr     { $$ = binop_expr($1, "&&", $3); }
    | expr EC expr      { $$ = binop_expr($1, "&", $3); }
    | expr SHIFTL expr  { $$ = binop_expr($1, "<<", $3); }
    | expr SHIFTR expr  { $$ = binop_expr($1, ">>", $3); }
;

%%

extern int yylineno;
extern char *yytext;

void yyerror(const char *s) {
    fprintf(stderr, "Erro sintático na linha %d: %s (próximo a '%s')\n", yylineno, s, yytext);
}