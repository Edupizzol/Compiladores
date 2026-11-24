#ifndef OPTIMIZE_H
#define OPTIMIZE_H

#include "ast.h"

/* constant folding (#49). Se os operandos da expressao `e` sao literais,
   troca a subarvore por um unico literal com o resultado e o tipo certos:
   `2 + 3` vira `expr -> INT_NUMBER (5)`. O codegen (#54) gera o Rust da
   arvore ja dobrada.

   Os filhos de `e` tem que estar dobrados antes. O parser chama esta funcao
   em cada `expr` que reduz (se o -O estiver ligado), e como o Bison reduz de
   baixo pra cima, isso e um percurso em pos-ordem: `(1+2)*(3+4)` dobra o
   `1+2`, o `3+4`, os parenteses e por fim o `*`, e chega em `21`.

   Nao dobra o que mudaria o comportamento do programa: divisao ou `%` por
   zero, overflow de int, shift negativo ou >= 32, resultado float infinito
   ou NaN, e expressao com erro de tipo. */
void fold_constants(Node *e);

#endif
