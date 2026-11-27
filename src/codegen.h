#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include "ast.h"

/* gera o Rust do programa a partir da arvore de derivacao ja anotada com
   os tipos (#44) e, com -O, ja otimizada (#49), e escreve em `out` (#54).
   Percorre os comandos em pre-ordem e as expressoes em pos-ordem.

   Como o tac_generate, precisa da tabela de simbolos ainda viva (usa
   symtab_lookup_func pra assinatura das funcoes) e so deve ser chamado pra
   programa sem erro lexico, sintatico nem semantico. */
void codegen_rust(const Node *root, FILE *out);

#endif
