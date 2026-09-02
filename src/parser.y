%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"

void yyerror(const char *s);
int yylex(void);
%}

%union {
    DataType data_type;
    ValueData val;
    char *str;
}

%token <val> IDENTIFIER INT_NUMBER FLOAT_NUMBER
%token <data_type> TYPE_KW
%token RETURN ASSIGN SEMICOLON LBRACE RBRACE LPAREN RPAREN
%token PLUS MINUS STAR MOD XOR OR BINOR AND EC SHIFTL SHIFTR

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

%%

void yyerror(const char *s) {
    fprintf(stderr, "Erro sintático: %s\n", s);
}

int main(void) {
    return yyparse();
}