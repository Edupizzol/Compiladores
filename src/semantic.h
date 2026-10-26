#ifndef SEMANTIC_H
#define SEMANTIC_H

/* quantos erros semanticos o parse encontrou ate agora */
extern int semantic_errors;

/* reporta um erro semantico em stderr, no mesmo formato do yyerror:
   `Erro semântico na linha N: <mensagem>`, e conta em semantic_errors.
   Nao interrompe o parse. */
void semantic_error(int line, const char *fmt, ...);

#endif
