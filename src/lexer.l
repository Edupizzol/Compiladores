%{
#include "types.h"
#include "parser.tab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
%}

%option noyywrap
%option yylineno

%%

"int"           { yylval.data_type = TYPE_INT; return TYPE_KW; }
"float"         { yylval.data_type = TYPE_FLOAT; return TYPE_KW; }
"char"          { yylval.data_type = TYPE_CHAR; return TYPE_KW; }

"return"        { return RETURN; }

"if"            { return IF; }
"else"          { return ELSE; }
"while"         { return WHILE; }
"for"           { return FOR; }
"break"         { return BREAK; }
"continue"      { return CONTINUE; }

[0-9]+\.[0-9]+  { yylval.val.f_val = atof(yytext); return FLOAT_NUMBER; }
[0-9]+          { yylval.val.i_val = atoi(yytext); return INT_NUMBER; }
[a-zA-Z_][a-zA-Z0-9_]* { yylval.val.s_val = strdup(yytext); return IDENTIFIER; }

"=="            { return COMP; }
"="             { return ASSIGN; }
";"             { return SEMICOLON; }
","             { return ','; }
"{"             { return LBRACE; }
"}"             { return RBRACE; }
"("             { return LPAREN; }
")"             { return RPAREN; }
"^"             { return XOR; }
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
[ \t\n\r]+      /* ignora espaços */ ;
.               { printf("Caractere desconhecido: %s\n", yytext); }

%%