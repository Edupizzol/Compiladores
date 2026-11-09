#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "types.h"

/* tipos numericos de C (int, float, double, char): os unicos que entram em
   aritmetica. void nao e valor */
int type_is_numeric(DataType t);

/* tipos inteiros (int, char): os unicos aceitos em % << >> & | ^ */
int type_is_integer(DataType t);

/* conversoes aritmeticas usuais de C: char vira int, e entre os demais vence
   o maior (int < float < double). Os dois tipos tem que ser numericos */
DataType type_promote(DataType a, DataType b);

/* resolve, no fim do parse, o tipo das expressoes que dependiam de uma
   funcao definida so depois do uso (TYPE_UNKNOWN), agora que a tabela tem
   todas as assinaturas. Nao gera erro novo: so deixa a arvore anotada
   completa pro --types e pro TAC */
struct Node;
void semantic_resolve_types(struct Node *root);

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

/* tipos dos argumentos de uma chamada a funcao ainda nao definida. As
   chamadas a funcoes ja conhecidas tem os argumentos checados na hora, no
   parser; estas so podem ser checadas no fim. Chamar logo depois do
   semantic_call da mesma chamada. `types` e copiado */
void semantic_call_arg_types(const DataType *types, int n);

/* confere as chamadas guardadas contra a tabela (funcao declarada e numero
   de argumentos) e libera a lista. Rodar com o parse completo. */
void semantic_check_calls(void);

#endif
