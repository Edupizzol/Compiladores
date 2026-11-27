#ifndef BYTECODE_H
#define BYTECODE_H

#include <stdio.h>
#include "ast.h"

/* bytecode de maquina de pilha (issue #56) de um programa ja analisado: uma
   instrucao por linha, em maiusculas, e o valor de cada expressao fica no
   topo da pilha.

   - LOAD_CONST v, LOAD x, STORE x (x, x.1, x.2 nos escopos aninhados, como no TAC)
   - ADD SUB MUL DIV MOD, BIT_AND BIT_OR BIT_XOR SHL SHR, NEG
   - EQ NE LT GT LE GE e NOT (dao 0 ou 1)
   - I2F I2D F2I F2D D2I D2F: conversao entre int/char, float e double
   - LABEL L, JUMP L, JUMP_IF_FALSE L (desempilha o valor); && e || com
     curto-circuito, break e continue viram JUMP
   - CALL f n: o chamador empilha os n argumentos em ordem e a funcao os
     tira com STORE, do ultimo pro primeiro; RET devolve o valor do topo
   - POP descarta um valor, mas o subconjunto nao tem expressao como
     comando, entao nenhum codigo gera POP por enquanto

   Cada funcao comeca com `LABEL nome`, e a void termina com `RET`. Nao deve
   ser chamado se o programa teve erro lexico, sintatico ou semantico. */
void bytecode_generate(const Node *root, FILE *out);

#endif