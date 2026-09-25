%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"

void yyerror(const char *s);
int yylex(void);
extern FILE *yyin;
%}

%union {
    DataType data_type;
    ValueData val;
    char *str;
}

%token <val> IDENTIFIER INT_NUMBER FLOAT_NUMBER
%token <data_type> TYPE_KW
%token RETURN ASSIGN SEMICOLON LBRACE RBRACE LPAREN RPAREN COMMA
%token PLUS MINUS STAR SLASH MOD XOR OR BINOR AND EC SHIFTL SHIFTR
%token COMP NE LT GT LE GE NOT
%token UMINUS

%type <str> statement_list statement expr postfix_expr primary_expr arg_list
%type <data_type> type_specifier

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
%right NOT UMINUS

%%

program:
    type_specifier IDENTIFIER LPAREN RPAREN LBRACE statement_list RBRACE {
        printf("%s %s() {\n%s}\n", type_to_cpp($1), $2.s_val, $6);
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
        char *buf = malloc(strlen(type_to_cpp($1)) + strlen($2.s_val) + strlen($4) + 32);
        sprintf(buf, "    %s %s = %s;\n", type_to_cpp($1), $2.s_val, $4);
        $$ = buf;
    }
    | IDENTIFIER ASSIGN expr SEMICOLON {
        char *buf = malloc(strlen($1.s_val) + strlen($3) + 32);
        sprintf(buf, "    %s = %s;\n", $1.s_val, $3);
        $$ = buf;
    }
    | RETURN expr SEMICOLON {
        char *buf = malloc(strlen($2) + 32);
        sprintf(buf, "    return %s;\n", $2);
        $$ = buf;
    }
;

primary_expr:
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
        $$ = $1.s_val;
    }
    | LPAREN expr RPAREN {
        char *buf = malloc(strlen($2) + 4);
        sprintf(buf, "(%s)", $2);
        $$ = buf;
    }
;

postfix_expr:
    primary_expr { $$ = $1; }
    | postfix_expr LPAREN RPAREN {
        char *buf = malloc(strlen($1) + 4);
        sprintf(buf, "%s()", $1);
        $$ = buf;
    }
    | postfix_expr LPAREN arg_list RPAREN {
        char *buf = malloc(strlen($1) + strlen($3) + 4);
        sprintf(buf, "%s(%s)", $1, $3);
        $$ = buf;
    }
;

arg_list:
    expr { $$ = $1; }
    | arg_list COMMA expr {
        char *buf = malloc(strlen($1) + strlen($3) + 4);
        sprintf(buf, "%s, %s", $1, $3);
        $$ = buf;
    }
;

expr:
    postfix_expr { $$ = $1; }
    | expr PLUS expr {
        char *buf = malloc(strlen($1) + strlen($3) + 4);
        sprintf(buf, "%s + %s", $1, $3);
        $$ = buf;
    }
    | expr MINUS expr {
        char *buf = malloc(strlen($1) + strlen($3) + 4);
        sprintf(buf, "%s - %s", $1, $3);
        $$ = buf;
    }
    | expr STAR expr {
        char *buf = malloc(strlen($1) + strlen($3) + 4);
        sprintf(buf, "%s * %s", $1, $3);
        $$ = buf;
    }
    | expr SLASH expr {
        char *buf = malloc(strlen($1) + strlen($3) + 4);
        sprintf(buf, "%s / %s", $1, $3);
        $$ = buf;
    }
    | expr MOD expr {
        char *buf = malloc(strlen($1) + strlen($3) + 4);
        sprintf(buf, "%s %% %s", $1, $3);
        $$ = buf;
    }
    | expr LT expr {
        char *buf = malloc(strlen($1) + strlen($3) + 4);
        sprintf(buf, "%s < %s", $1, $3);
        $$ = buf;
    }
    | expr GT expr {
        char *buf = malloc(strlen($1) + strlen($3) + 4);
        sprintf(buf, "%s > %s", $1, $3);
        $$ = buf;
    }
    | expr LE expr {
        char *buf = malloc(strlen($1) + strlen($3) + 5);
        sprintf(buf, "%s <= %s", $1, $3);
        $$ = buf;
    }
    | expr GE expr {
        char *buf = malloc(strlen($1) + strlen($3) + 5);
        sprintf(buf, "%s >= %s", $1, $3);
        $$ = buf;
    }
    | expr COMP expr {
        char *buf = malloc(strlen($1) + strlen($3) + 5);
        sprintf(buf, "%s == %s", $1, $3);
        $$ = buf;
    }
    | expr NE expr {
        char *buf = malloc(strlen($1) + strlen($3) + 5);
        sprintf(buf, "%s != %s", $1, $3);
        $$ = buf;
    }
    | expr AND expr {
        char *buf = malloc(strlen($1) + strlen($3) + 5);
        sprintf(buf, "%s && %s", $1, $3);
        $$ = buf;
    }
    | expr OR expr {
        char *buf = malloc(strlen($1) + strlen($3) + 5);
        sprintf(buf, "%s || %s", $1, $3);
        $$ = buf;
    }
    | expr EC expr {
        char *buf = malloc(strlen($1) + strlen($3) + 4);
        sprintf(buf, "%s & %s", $1, $3);
        $$ = buf;
    }
    | expr BINOR expr {
        char *buf = malloc(strlen($1) + strlen($3) + 4);
        sprintf(buf, "%s | %s", $1, $3);
        $$ = buf;
    }
    | expr XOR expr {
        char *buf = malloc(strlen($1) + strlen($3) + 4);
        sprintf(buf, "%s ^ %s", $1, $3);
        $$ = buf;
    }
    | expr SHIFTL expr {
        char *buf = malloc(strlen($1) + strlen($3) + 5);
        sprintf(buf, "%s << %s", $1, $3);
        $$ = buf;
    }
    | expr SHIFTR expr {
        char *buf = malloc(strlen($1) + strlen($3) + 5);
        sprintf(buf, "%s >> %s", $1, $3);
        $$ = buf;
    }
    | NOT expr {
        char *buf = malloc(strlen($2) + 3);
        sprintf(buf, "!%s", $2);
        $$ = buf;
    }
    | MINUS expr %prec UMINUS {
        char *buf = malloc(strlen($2) + 3);
        sprintf(buf, "-%s", $2);
        $$ = buf;
    }
;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Erro sintático: %s\n", s);
}

int main(int argc, char **argv) {
    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) {
            fprintf(stderr, "Não foi possível abrir o arquivo: %s\n", argv[1]);
            return 1;
        }
    }
    return yyparse();
}