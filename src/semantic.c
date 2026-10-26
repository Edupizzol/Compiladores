#include <stdarg.h>
#include <stdio.h>

#include "semantic.h"

int semantic_errors = 0;

void semantic_error(int line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Erro semântico na linha %d: ", line);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
    semantic_errors++;
}
