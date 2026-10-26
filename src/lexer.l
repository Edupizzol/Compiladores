%{
#include "types.h"
#include "parser.tab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* linha onde o comentario de bloco aberto comecou, pra reportar se nao fechar */
static int comment_start_line;

/* quantos erros lexicos apareceram: o programa e invalido mesmo que o
   parser consiga seguir (o caractere desconhecido e so pulado) */
int lexical_errors = 0;

/* os lexemas que vao pro parser (yylval.val.s_val) ficam guardados aqui e
   sao liberados todos juntos no fim (lexer_free_strings), entao as acoes do
   parser nao precisam liberar nada */
static char **lex_strings;
static int n_lex_strings, cap_lex_strings;

static char *lex_strdup(const char *s) {
    if (n_lex_strings == cap_lex_strings) {
        cap_lex_strings = cap_lex_strings ? cap_lex_strings * 2 : 64;
        lex_strings = realloc(lex_strings, cap_lex_strings * sizeof(char *));
    }
    return lex_strings[n_lex_strings++] = strdup(s);
}

void lexer_free_strings(void) {
    for (int i = 0; i < n_lex_strings; i++)
        free(lex_strings[i]);
    free(lex_strings);
    lex_strings = NULL;
    n_lex_strings = cap_lex_strings = 0;
}

/* linha de cada token pro parser (@N.first_line), ja que yylineno lido na
   acao do bison pode estar adiantado pelo lookahead */
#define YY_USER_ACTION yylloc.first_line = yylloc.last_line = yylineno;
%}

%option noyywrap
%option yylineno
%option nounput noinput

%x COMMENT

DIGIT   [0-9]
EXP     [eE][+-]?{DIGIT}+
/* sequencias de escape aceitas em literais de char/string: \n \t \0 \\ \' \" etc */
ESC     \\([ntr0abfv\\'"?]|x[0-9a-fA-F]+)

%%

 /* comentarios */
"//"[^\n]*      /* comentario de linha: ignora */ ;
"/*"            { comment_start_line = yylineno; BEGIN(COMMENT); }
<COMMENT>"*/"   { BEGIN(INITIAL); }
<COMMENT>[^*]+  /* yylineno conta os \n consumidos aqui */ ;
<COMMENT>"*"    ;
<COMMENT><<EOF>> {
                  fprintf(stderr, "Erro léxico na linha %d: comentário de bloco não fechado\n", comment_start_line); lexical_errors++;
                  BEGIN(INITIAL);
                  return 0;
                }

 /* tipos */
"int"           { yylval.data_type = TYPE_INT; return TYPE_KW; }
"float"         { yylval.data_type = TYPE_FLOAT; return TYPE_KW; }
"double"        { yylval.data_type = TYPE_DOUBLE; return TYPE_KW; }
"char"          { yylval.data_type = TYPE_CHAR; return TYPE_KW; }
"void"          { yylval.data_type = TYPE_VOID; return TYPE_KW; }
"struct"        { return STRUCT; }

 /* palavras reservadas de controle */
"return"        { return RETURN; }
"if"            { return IF; }
"else"          { return ELSE; }
"while"         { return WHILE; }
"for"           { return FOR; }
"break"         { return BREAK; }
"continue"      { return CONTINUE; }

 /* literais numericos: o valor vai pro codegen e o lexema (s_val) pra folha da arvore */
{DIGIT}+\.{DIGIT}*{EXP}?  { yylval.val.f_val = atof(yytext); yylval.val.s_val = lex_strdup(yytext); return FLOAT_NUMBER; }
\.{DIGIT}+{EXP}?          { yylval.val.f_val = atof(yytext); yylval.val.s_val = lex_strdup(yytext); return FLOAT_NUMBER; }
{DIGIT}+{EXP}             { yylval.val.f_val = atof(yytext); yylval.val.s_val = lex_strdup(yytext); return FLOAT_NUMBER; }
{DIGIT}+                  { yylval.val.i_val = atoi(yytext); yylval.val.s_val = lex_strdup(yytext); return INT_NUMBER; }

 /* literais de char e string: guardam o texto com as aspas, que ja e valido em Rust ('\0', "a\n") */
\'([^'\\\n]|{ESC})\'      { yylval.val.s_val = lex_strdup(yytext); return CHAR_LITERAL; }
\"([^"\\\n]|{ESC})*\"     { yylval.val.s_val = lex_strdup(yytext); return STRING_LITERAL; }
 /* fallback: string/char sem fechar, vazio, com mais de um char ou escape desconhecido */
\"([^"\\\n]|\\.)*\"?      { fprintf(stderr, "Erro léxico na linha %d: string inválida ou não fechada: %s\n", yylineno, yytext); lexical_errors++; }
\'([^'\\\n]|\\.)*\'?      { fprintf(stderr, "Erro léxico na linha %d: literal de char inválido: %s\n", yylineno, yytext); lexical_errors++; }

[a-zA-Z_][a-zA-Z0-9_]*    { yylval.val.s_val = lex_strdup(yytext); return IDENTIFIER; }

 /* atribuicao composta, incremento e decremento */
"++"            { return INC; }
"--"            { return DEC; }
"+="            { return ADD_ASSIGN; }
"-="            { return SUB_ASSIGN; }
"*="            { return MUL_ASSIGN; }
"/="            { return DIV_ASSIGN; }
"%="            { return MOD_ASSIGN; }
"&="            { return AND_ASSIGN; }
"|="            { return OR_ASSIGN; }
"^="            { return XOR_ASSIGN; }
"<<="           { return SHL_ASSIGN; }
">>="           { return SHR_ASSIGN; }

 /* relacionais */
"=="            { return COMP; }
"!="            { return NE; }
"<="            { return LE; }
">="            { return GE; }
"<"             { return LT; }
">"             { return GT; }

 /* acesso a membro de struct */
"->"            { return ARROW; }
"."             { return DOT; }

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
"!"             { return NOT; }
"%"             { return MOD; }
"<<"            { return SHIFTL; }
">>"            { return SHIFTR; }
"+"             { return PLUS; }
"-"             { return MINUS; }
"*"             { return STAR; }
"/"             { return SLASH; }
[ \t\n\r]+      /* ignora espaços (yylineno conta as linhas) */ ;
.               { fprintf(stderr, "Erro léxico na linha %d: caractere desconhecido '%s'\n", yylineno, yytext); lexical_errors++; }

%%