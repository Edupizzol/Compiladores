#include <stdio.h>

int yyparse(void);
extern FILE *yyin;

int main(int argc, char **argv) {
    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) {
            perror("fopen");
            return 1;
        }
    }
    int ret = yyparse();
    if (argc > 1 && yyin) fclose(yyin);
    return ret;
}