%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"

void yyerror(const char *s);
int yylex(void);

static char *binop_expr(char *lhs, const char *op, char *rhs) {
    char *buf = malloc(strlen(lhs) + strlen(op) + strlen(rhs) + 4);
    sprintf(buf, "%s %s %s", lhs, op, rhs);
    return buf;
}
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

%type <str> statement_list statement expr
%type <data_type> type_specifier

%left OR
%left AND
%left BINOR
%left XOR
%left EC
%left SHIFTL SHIFTR
%left PLUS MINUS
%left STAR MOD

%%

program:
    type_specifier IDENTIFIER LPAREN RPAREN LBRACE statement_list RBRACE {
        printf("fn %s() -> %s {\n%s}\n", $2.s_val, type_to_rust($1), $6);
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
    type_specifier IDENTIFIER ASSIGN expr SEMICOLON {
        char *buf = malloc(strlen($2.s_val) + strlen($4) + strlen(type_to_rust($1)) + 32);
        sprintf(buf, "    let mut %s: %s = %s;\n", $2.s_val, type_to_rust($1), $4);
        $$ = buf;
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

void yyerror(const char *s) {
    fprintf(stderr, "Erro sintático: %s\n", s);
}

int main(void) {
    return yyparse();
}