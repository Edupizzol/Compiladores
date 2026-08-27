%{
#include "parser.tab.h"
#include <stdio.h>
#include <string.h>
%}

%option noyywrap

%%

"int"           { return INT; }
"return"        { return RETURN; }
[a-zA-Z_][a-zA-Z0-9_]* { yylval.str = strdup(yytext); return IDENTIFIER; }
[0-9]+          { yylval.num = atoi(yytext); return NUMBER; }
"="             { return ASSIGN; }
";"             { return SEMICOLON; }
"{"             { return LBRACE; }
"}"             { return RBRACE; }
"("             { return LPAREN; }
")"             { return RPAREN; }
"^"             { return XOR;   }
"||"            { return OR; }
"|"             { return BINOR; }
"&&"            { return AND; }
"&"             { return EC; }
"%"             { return MOD; }
"<<"            { return SHIFTL; }
">>"            { return SHIFTR; }
"+"             { return PLUS; }
"-"             { return MINUS; }
"*"             { return STAR; }
[ \t\n\r]+      /* ignora espaços em branco */ ;
.               { printf("Caractere desconhecido: %s\n", yytext); }

%%