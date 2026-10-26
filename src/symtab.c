#include <stdlib.h>
#include <string.h>

#include "symtab.h"

/*
 * Implementacao da tabela de simbolos.
 *
 * A estrutura tem tres partes:
 *
 *  - uma pilha de escopos (ScopeNode), com o topo sendo o escopo atual;
 *  - uma tabela hash por nome, que guarda TODOS os simbolos de todos os
 *    escopos;
 *  - uma lista de todos os escopos ja criados, do mais antigo pro mais
 *    novo, que e o historico.
 *
 * Cada simbolo carrega o `scope_id` e o `level` do escopo em que foi
 * declarado. Isso deixa as duas operacoes da API diretas:
 *
 *   - symtab_lookup_current so olha os simbolos do escopo atual;
 *   - symtab_lookup desce a pilha de escopos do mais interno pro global e,
 *     em cada um, procura o nome. Um nome declarado num escopo externo fica
 *     escondido por um do interno (sombreamento), e um simbolo de um escopo
 *     ja fechado nao aparece em escopos irmaos.
 *
 * Fechar um escopo (scope_exit) nao joga os simbolos fora: eles continuam na
 * hash e o no de escopo continua no historico. E assim que a tabela completa
 * pode ser impressa depois do parse (#25).
 *
 * Cada escopo guarda tambem a lista dos seus simbolos na ordem de
 * declaracao, pra que a impressao da tabela seja estavel (#25).
 */

#define SYMTAB_SIZE 211   /* primo: espalha melhor os nomes */

typedef struct ScopeNode ScopeNode;

struct ScopeNode {
    Scope scope;
    ScopeNode *parent;    /* escopo pai (NULL no global) */
    ScopeNode *next_all;  /* todos os escopos, do mais antigo pro mais novo */
    Symbol *decl_head;    /* simbolos do escopo, na ordem de declaracao */
    Symbol *decl_tail;
};

/* hash do nome: djb2 */
static unsigned hash_name(const char *name) {
    unsigned h = 5381;
    for (; *name; name++)
        h = h * 33 + (unsigned char)*name;
    return h % SYMTAB_SIZE;
}

static Symbol *table[SYMTAB_SIZE];
static ScopeNode *scope_stack = NULL;   /* topo = escopo atual */
static ScopeNode *scope_list = NULL;    /* todos os escopos, do mais antigo */
static int next_scope_id = 1;           /* o global ja usou o id 0 */

void symtab_init(void) {
    memset(table, 0, sizeof(table));
    scope_stack = NULL;
    scope_list = NULL;
    next_scope_id = 1;

    /* o escopo global: level 0, id 0 */
    ScopeNode *global = calloc(1, sizeof(ScopeNode));
    global->scope.scope_id = 0;
    global->scope.level = 0;
    global->next_all = NULL;
    scope_stack = global;
    scope_list = global;
}

int scope_enter(void) {
    ScopeNode *node = calloc(1, sizeof(ScopeNode));
    node->scope.scope_id = next_scope_id++;
    node->scope.level = scope_level() + 1;
    node->parent = scope_stack;
    node->next_all = NULL;

    /* empilha na lista de todos os escopos (ordem de criacao) */
    ScopeNode **tail = &scope_list;
    while (*tail) tail = &(*tail)->next_all;
    *tail = node;

    scope_stack = node;
    return node->scope.scope_id;
}

void scope_exit(void) {
    /* o global nao fecha */
    if (!scope_stack || !scope_stack->parent) return;

    /* o no continua vivo no historico -- so sai da pilha */
    scope_stack = scope_stack->parent;
}

int scope_level(void) {
    return scope_stack ? scope_stack->scope.level : 0;
}

int scope_current_id(void) {
    return scope_stack ? scope_stack->scope.scope_id : 0;
}

/* procura `name` so no escopo de id `scope_id` */
static Symbol *lookup_in(const char *name, int scope_id) {
    if (!name) return NULL;
    for (Symbol *sym = table[hash_name(name)]; sym; sym = sym->next) {
        if (sym->scope_id != scope_id) continue;
        if (strcmp(sym->name, name) == 0) return sym;
    }
    return NULL;
}

/* cria o simbolo e o poe na hash e na lista de declaracao de `scope` */
static Symbol *insert_in(ScopeNode *scope, const char *name, SymKind kind, DataType type, int line) {
    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = strdup(name);
    sym->kind = kind;
    sym->type = type;
    sym->scope_id = scope->scope.scope_id;
    sym->level = scope->scope.level;
    sym->line = line;
    sym->n_params = 0;
    sym->param_types = NULL;
    sym->next = NULL;
    sym->next_decl = NULL;

    unsigned h = hash_name(name);
    sym->next = table[h];
    table[h] = sym;

    /* lista de declaracao do escopo, pra impressao estavel */
    if (scope->decl_tail) scope->decl_tail->next_decl = sym;
    else scope->decl_head = sym;
    scope->decl_tail = sym;

    return sym;
}

int symtab_insert(const char *name, SymKind kind, DataType type, int line) {
    /* ja existe no escopo atual? ai e falha, sem imprimir nada */
    if (symtab_lookup_current(name)) return -1;

    insert_in(scope_stack, name, kind, type, line);
    return 0;
}

int symtab_insert_func(const char *name, DataType ret, int n_params,
                       const DataType *param_types, int line) {
    /* o global e sempre o primeiro da lista de escopos (id 0) */
    if (lookup_in(name, 0)) return -1;

    Symbol *sym = insert_in(scope_list, name, SYM_FUNC, ret, line);
    sym->n_params = n_params;
    if (n_params > 0) {
        sym->param_types = malloc(n_params * sizeof(DataType));
        memcpy(sym->param_types, param_types, n_params * sizeof(DataType));
    }
    return 0;
}

Symbol *symtab_lookup_func(const char *name) {
    Symbol *sym = lookup_in(name, 0);
    return sym && sym->kind == SYM_FUNC ? sym : NULL;
}

Symbol *symtab_lookup(const char *name) {
    if (!name) return NULL;

    /* do escopo mais interno pro global */
    for (ScopeNode *scope = scope_stack; scope; scope = scope->parent) {
        for (Symbol *sym = table[hash_name(name)]; sym; sym = sym->next) {
            if (sym->scope_id != scope->scope.scope_id) continue;
            if (strcmp(sym->name, name) == 0) return sym;
        }
    }
    return NULL;
}

Symbol *symtab_lookup_current(const char *name) {
    return lookup_in(name, scope_current_id());
}

int symtab_scope_count(int scope_id) {
    int count = 0;
    for (int i = 0; i < SYMTAB_SIZE; i++) {
        for (Symbol *sym = table[i]; sym; sym = sym->next) {
            if (sym->scope_id == scope_id) count++;
        }
    }
    return count;
}

void symtab_foreach(void (*fn)(Symbol *sym, void *ctx), void *ctx) {
    if (!fn) return;

    /* percorre todos os escopos na ordem de criacao (o global primeiro, e o
       scope_id e crescente), e dentro de cada um os simbolos na ordem de
       declaracao. Inclui escopos ja fechados, que e o que a impressao da
       tabela completa (#25) precisa */
    for (ScopeNode *s = scope_list; s; s = s->next_all) {
        for (Symbol *sym = s->decl_head; sym; sym = sym->next_decl) {
            fn(sym, ctx);
        }
    }
}

void symtab_free(void) {
    for (int i = 0; i < SYMTAB_SIZE; i++) {
        Symbol *sym = table[i];
        while (sym) {
            Symbol *next = sym->next;
            free(sym->name);
            free(sym->param_types);
            free(sym);
            sym = next;
        }
        table[i] = NULL;
    }

    ScopeNode *node = scope_list;
    while (node) {
        ScopeNode *next = node->next_all;
        free(node);
        node = next;
    }
    scope_list = NULL;
    scope_stack = NULL;
}
