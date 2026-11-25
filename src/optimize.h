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

/* simplificacoes algebricas (#50). Roda depois do fold_constants na mesma
   expressao, no mesmo percurso pos-ordem, pra pegar coisas como `x*(3-2)`
   depois que `3-2` vira `1`.

   Identidades aplicadas (so com um dos lados literal):
   - neutro: `x+0`, `0+x`, `x-0`, `x*1`, `1*x`, `x/1`, `x|0`, `x^0`,
     `x<<0`, `x>>0` -> `x` (com cast quando a expressao mudou de tipo,
     ex. `x+0.0` com `x` int vira `(x as f64)`);
   - absorvente: `x*0`, `x&0` -> `0`, so com `x` inteiro e sem efeito
     colateral (chamada de funcao);
   - reducao de forca: `x*2` -> `x<<1` (so inteiro; mantem a avaliacao
     unica do operando, entao vale mesmo com `x` com efeito colateral).

   Nao mexe em `x-x`, `x/x` nem em `x*0.0` (NaN e divisao por zero quebram
   as identidades no float). */
void simplify_algebraic(Node *e);

#endif
