#ifndef OPTIONS_H
#define OPTIONS_H

/* flags de CLI, combinaveis entre si */
#define OPT_TOKENS  (1 << 0)
#define OPT_TREE    (1 << 1)
#define OPT_DOT     (1 << 2)
#define OPT_SYMTAB  (1 << 3)

/* flags ativas nesta execucao (bitmask) */
extern int g_options;

/* o codegen (Rust) so e impresso quando nenhuma flag de analise esta ligada:
   com --tree, --dot ou --symtab a saida e a analise, nao o codigo */
extern int emit_code;

#endif
