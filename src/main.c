#include <stdio.h>
#include <string.h>

int yyparse(void);
int yylex(void);
const char *token_name(int tok);
extern FILE *yyin;
extern char *yytext;
extern int yylineno;

/* modo --tokens: so roda o lexico e lista os tokens, sem passar pelo parser */
static int dump_tokens(void) {
    int tok;
    while ((tok = yylex()) != 0)
        printf("%4d  %-16s %s\n", yylineno, token_name(tok), yytext);
    return 0;
}

int main(int argc, char **argv) {
    int tokens_only = 0;
    if (argc > 1 && strcmp(argv[1], "--tokens") == 0) {
        tokens_only = 1;
        argv++;
        argc--;
    }
    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) {
            perror("fopen");
            return 1;
        }
    }
    int ret = tokens_only ? dump_tokens() : yyparse();
    if (argc > 1 && yyin) fclose(yyin);
    return ret;
}
