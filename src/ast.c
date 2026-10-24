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
