#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

Node *parse_root = NULL;

Node *node_new(const char *label, int line) {
    Node *n = calloc(1, sizeof(Node));
    n->label = label;
    n->line = line;
    return n;
}

Node *node_leaf(const char *label, const char *text, int line) {
    Node *n = node_new(label, line);
    n->text = text ? strdup(text) : NULL;
    return n;
}

void node_add_child(Node *parent, Node *child) {
    if (!child) return;
    if (parent->n_children == parent->cap) {
        parent->cap = parent->cap ? parent->cap * 2 : 4;
        parent->children = realloc(parent->children, parent->cap * sizeof(Node *));
    }
    parent->children[parent->n_children++] = child;
}

void node_free(Node *root) {
    if (!root) return;
    for (int i = 0; i < root->n_children; i++)
        node_free(root->children[i]);
    free(root->children);
    free(root->text);
    free(root->code);
    free(root);
}

/* rotulo de um no como aparece na saida: `LABEL (text)` se tiver lexema */
static void print_label(const Node *n, FILE *out) {
    if (n->text) fprintf(out, "%s (%s)", n->label, n->text);
    else fputs(n->label, out);
}

/* `prefix` e o que vem antes do ramo desta linha: um "│   " por ancestral
   que ainda tem irmaos depois, "    " pros que nao tem */
static void print_children(const Node *n, FILE *out, char *prefix, size_t len) {
    for (int i = 0; i < n->n_children; i++) {
        const Node *c = n->children[i];
        int last = i == n->n_children - 1;
        fprintf(out, "%s%s", prefix, last ? "└── " : "├── ");
        print_label(c, out);
        fputc('\n', out);

        const char *ext = last ? "    " : "│   ";
        size_t ext_len = strlen(ext);
        char *child_prefix = malloc(len + ext_len + 1);
        memcpy(child_prefix, prefix, len);
        memcpy(child_prefix + len, ext, ext_len + 1);
        print_children(c, out, child_prefix, len + ext_len);
        free(child_prefix);
    }
}

void tree_print(const Node *root, FILE *out) {
    if (!root) return;
    print_label(root, out);
    fputc('\n', out);
    char empty[1] = "";
    print_children(root, out, empty, 0);
}

/* escreve `s` entre aspas, escapando o que o DOT nao aceita cru */
static void dot_string(const char *s, FILE *out) {
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') fputc('\\', out);
        fputc(*s, out);
    }
}

/* emite o no `n` com o id `*next_id` e as arestas pros filhos; devolve o
   id dado a `n` */
static int dot_node(const Node *n, FILE *out, int *next_id) {
    int id = (*next_id)++;
    fprintf(out, "    n%d [label=\"", id);
    dot_string(n->label, out);
    if (n->text) {
        fputs(" (", out);
        dot_string(n->text, out);
        fputc(')', out);
    }
    /* nao-terminal sempre tem filho (producao vazia tem o ε), entao folha
       e terminal */
    fprintf(out, "\", shape=%s];\n", n->n_children ? "ellipse" : "box");
    for (int i = 0; i < n->n_children; i++) {
        int child = dot_node(n->children[i], out, next_id);
        fprintf(out, "    n%d -> n%d;\n", id, child);
    }
    return id;
}

void tree_to_dot(const Node *root, FILE *out) {
    fputs("digraph arvore {\n", out);
    fputs("    node [fontname=\"Helvetica\"];\n", out);
    if (root) {
        int next_id = 0;
        dot_node(root, out, &next_id);
    }
    fputs("}\n", out);
}
