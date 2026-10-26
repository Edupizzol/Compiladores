#ifndef SYMTAB_DUMP_H
#define SYMTAB_DUMP_H

#include <stdio.h>
#include "symtab.h"

/* imprime a tabela de simbolos completa em `out` (--symtab): todos os
   escopos, inclusive os ja fechados, no formato do symtab_print */
void symtab_dump(FILE *out);

/* imprime a tabela de simbolos em `out`, uma linha por simbolo, ordenada
   por scope_id e, dentro do mesmo escopo, pela ordem em que aparecem em
   `syms` (ordem de declaracao). Colunas alinhadas:
   ESCOPO NIVEL NOME CATEGORIA TIPO LINHA. Nao altera `syms`. */
void symtab_print(FILE *out, Symbol *const *syms, int n);

/* escreve em `buf` o tipo do simbolo em C: `int` pra variavel/parametro,
   assinatura `int (int, float)` pra funcao (`int (void)` sem parametros).
   Devolve `buf`. */
char *symbol_type_str(const Symbol *s, char *buf, size_t size);

#endif
