%{
#include <stdio.h>
#include <stdlib.h>
#include "types.h"
''
void yyerror(const char *s);
int yylex(void);
%}

%union {
    int num;
    char *str;
}

%token <str> IDENTIFIER
%token <num> NUMBER
%token INT RETURN ASSIGN SEMICOLON LBRACE RBRACE LPAREN RPAREN

%%

/* Regra principal: converte a função main de C para fn main em Rust */
program:
    INT IDENTIFIER LPAREN RPAREN LBRACE statement_list RBRACE {
        printf("fn main() {\n%s}\n", $6);
    }
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
    INT IDENTIFIER ASSIGN NUMBER SEMICOLON {
        char *buf = malloc(256);
        // int x = 10; em C vira let mut x: i32 = 10; em Rust
        sprintf(buf, "    let mut %s: i32 = %d;\n", $2, $4);
        $$ = buf;
    }
    | RETURN NUMBER SEMICOLON {
        char *buf = malloc(256);
        sprintf(buf, "    // return %d;\n", $2);
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