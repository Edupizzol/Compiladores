#ifndef AST_H
#define AST_H

/* No da arvore de derivacao. Cada regra do parser cria um no; o `code`
   guarda o trecho de Rust ja gerado para aquele no (o codegen atual ainda
   e dirigido pela sintaxe, entao ele continua vivendo aqui). */
typedef struct Node {
    const char *label;      /* nao-terminal ou token (literal, nao e copiado) */
    char *text;             /* lexema, so nas folhas (NULL nos demais) */
    int line;               /* linha do fonte onde o no comeca */
    char *code;             /* trecho de Rust deste no (NULL se nao houver) */
    struct Node **children;
    int n_children, cap;
} Node;

/* raiz da arvore, atribuida na acao de `program:`; liberada pelo main */
extern Node *parse_root;

/* cria um no interno sem filhos nem codigo. `label` deve viver ate o fim
   do programa (use string literal). */
Node *node_new(const char *label, int line);

/* cria uma folha de token: `text` e copiado com strdup */
Node *node_leaf(const char *label, const char *text, int line);

/* anexa `child` ao fim dos filhos de `parent`. O pai passa a ser dono do
   filho (node_free libera os dois). Ignora child == NULL. */
void node_add_child(Node *parent, Node *child);

/* libera o no, o text, o code e todos os descendentes (recursivo).
   Aceita NULL. */
void node_free(Node *root);

#endif
