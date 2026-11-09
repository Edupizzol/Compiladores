#ifndef TAC_H
#define TAC_H

#include <stdio.h>
#include "ast.h"

/* gera o codigo de tres enderecos (issue #45) de um programa ja analisado e
   escreve em `out`. Percorre a arvore (que tem que estar com os tipos
   anotados, #44) e precisa da tabela de simbolos ainda viva, pra achar a
   assinatura das funcoes chamadas. Nao deve ser chamado se o programa teve
   erro lexico, sintatico ou semantico.

   Uma funcao vira `func nome(params):` ... `endfunc`. Cada expressao
   gasta temporarios t0, t1, ... e cada salto usa rotulos L0, L1, ...; os
   dois recomecam a cada funcao. As conversoes implicitas de C aparecem como
   `t1 = (float) a`. */
void tac_generate(const Node *root, FILE *out);

#endif
