#ifndef SYMTAB_DUMP_H
#define SYMTAB_DUMP_H

#include <stdio.h>
#include "types.h"

/* ---- PROVISORIO ate a #21 entrar --------------------------------------
   Copia de SymKind/Symbol exatamente como especificado na #21. Quando o
   symtab.h real chegar, apagar este bloco e trocar por #include "symtab.h". */
typedef enum { SYM_VAR, SYM_PARAM, SYM_FUNC } SymKind;

typedef struct {
    char *name;
    SymKind kind;
    DataType type;          /* tipo da variavel/parametro, ou retorno da funcao */
    int scope_id;
    int level;
    int line;
    int n_params;           /* so pra SYM_FUNC (#23) */
    DataType *param_types;  /* so pra SYM_FUNC (#23) */
} Symbol;
/* ---- fim do bloco provisorio ------------------------------------------ */

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
