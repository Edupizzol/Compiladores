#ifndef TAC_OPT_H
#define TAC_OPT_H

#include <stdio.h>

/* otimizacao do TAC (issue #52), so com -O. Recebe o texto que o
   tac_generate escreveu e imprime em `out` a versao otimizada.

   Cada funcao e tratada sozinha, em quatro passos:
   1. rotulo que nenhum salto usa sai (o `continue` de um `for` sem
      `continue`, por exemplo), o que junta blocos basicos;
   2. dentro de cada bloco basico (trecho sem rotulo, sem salto e sem
      `call`), propagacao de constantes (`t0 = 3` e `t1 = t0 + 4` viram
      `t1 = 7`) e de copias (`a = t0` faz os usos de `a` virarem `t0` ate
      `a` ou `t0` mudarem). So a aritmetica inteira e dobrada, com as mesmas
      regras do #49 (nada de divisao por zero, overflow ou shift fora de
      0..31); constante float so e propagada;
   3. temporario que ninguem le e removido (`call` nunca sai);
   4. os temporarios e os rotulos que sobraram sao renumerados em ordem, a
      partir de t0 e L0.

   Nao propaga entre blocos basicos (precisaria de analise de fluxo). */
void tac_optimize(const char *text, FILE *out);

#endif
