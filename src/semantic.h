#ifndef SEMANTIC_H
#define SEMANTIC_H

/* quantos erros semanticos o parse encontrou ate agora */
extern int semantic_errors;

/* registra um erro semantico (`Erro semântico na linha N: <mensagem>`, o
   mesmo formato do yyerror) e conta em semantic_errors. Nao interrompe o
   parse. As mensagens ficam guardadas ate o semantic_flush(), porque as
   chamadas de funcao so sao checadas no fim do programa e o erro delas
   tem que sair na ordem das linhas junto com os outros. */
void semantic_error(int line, const char *fmt, ...);

/* imprime em stderr os erros guardados, ordenados por linha (os da mesma
   linha na ordem em que foram registrados), e libera a lista. Libera tambem
   as chamadas pendentes, se o parse parou antes de checa-las */
void semantic_flush(void);

/* chamada de funcao vista no parse, checada no fim do programa: a gramatica
   nao tem prototipos, entao a funcao pode ser definida depois do uso */
void semantic_call(const char *name, int n_args, int line);

/* confere as chamadas guardadas contra a tabela (funcao declarada e numero
   de argumentos) e libera a lista. Rodar com o parse completo. */
void semantic_check_calls(void);

#endif
