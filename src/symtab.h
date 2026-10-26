#ifndef SYMTAB_H
#define SYMTAB_H

#include "types.h"

/* O que o símbolo representa. */
typedef enum {
    SYM_VAR,    /* variável local ou global */
    SYM_PARAM,  /* parâmetro de função */
    SYM_FUNC    /* função */
} SymKind;

/*
 * Um símbolo da tabela. `scope_id` e `level` dizem a que escopo ele pertence:
 * `level` é a profundidade (0 = global), `scope_id` é o identificador único
 * e crescente que aquele escopo recebeu.
 *
 * `n_params` e `param_types` só fazem sentido pra SYM_FUNC (a assinatura é
 * montada na #23) — nos outros tipos ficam zerados.
 */
typedef struct Symbol {
    char *name;
    SymKind kind;
    DataType type;
    int scope_id;
    int level;
    int line;
    int n_params;
    DataType *param_types;
    struct Symbol *next;       /* próximo símbolo da mesma tabela (hash) */
    struct Symbol *next_decl;  /* próximo símbolo do mesmo escopo, por ordem
                                   de declaração */
} Symbol;

/* Identificação de um escopo: o id único e a profundidade. */
typedef struct Scope {
    int scope_id;
    int level;
} Scope;

/*
 * Inicializa a tabela de símbolos. Cria o escopo global (level 0) e zera a
 * contagem de escopos. Chame uma vez, antes de qualquer outra função.
 */
void symtab_init(void);

/*
 * Abre um escopo novo (um bloco, o corpo de uma função, ...) e devolve o
 * `scope_id` dele. O escopo global é o level 0 e não pode ser fechado.
 */
int scope_enter(void);

/*
 * Fecha o escopo atual. Os símbolos dele NÃO são descartados: ficam no
 * histórico, com o `scope_id` e o `level` de quando foram declarados, pra
 * que a tabela completa possa ser impressa depois do parse (#25).
 */
void scope_exit(void);

/* Profundidade do escopo atual: 0 no global, 1 no primeiro bloco, etc. */
int scope_level(void);

/* `scope_id` do escopo atual. */
int scope_current_id(void);

/*
 * Insere um símbolo no escopo atual.
 *
 * Devolve 0 em caso de sucesso e -1 se o nome já existe no escopo atual
 * (redeclaração no mesmo escopo). Nesse caso nada é inserido. Esconder um
 * nome de um escopo externo é permitido e também devolve 0.
 *
 * Não imprime nada: quem chama decide a mensagem de erro (#24).
 */
int symtab_insert(const char *name, SymKind kind, DataType type, int line);

/*
 * Insere uma função (SYM_FUNC) no escopo global, qualquer que seja o escopo
 * atual, com o tipo de retorno `ret` e a assinatura: `n_params` tipos em
 * `param_types` (o array é copiado).
 *
 * Devolve 0 em caso de sucesso e -1 se já existe um símbolo com esse nome
 * no global (função definida duas vezes). Nesse caso nada é inserido.
 */
int symtab_insert_func(const char *name, DataType ret, int n_params,
                       const DataType *param_types, int line);

/*
 * Procura uma função no escopo global e devolve o símbolo, com a
 * assinatura, ou NULL se não existir (ou se o nome não for uma função).
 */
Symbol *symtab_lookup_func(const char *name);

/*
 * Procura um símbolo do escopo mais interno pro global e devolve o
 * ponteiro, ou NULL se não existir. Devolve o primeiro (mais interno).
 */
Symbol *symtab_lookup(const char *name);

/*
 * Procura um símbolo só no escopo atual e devolve o ponteiro, ou NULL.
 */
Symbol *symtab_lookup_current(const char *name);

/* Devolve o número de símbolos declarados no escopo de `scope_id`. */
int symtab_scope_count(int scope_id);

/*
 * Percorre todos os símbolos da tabela, incluindo os de escopos já fechados,
 * e chama `fn` pra cada um. Os escopos vêm na ordem de criação (o global
 * primeiro, depois por `scope_id` crescente) e, dentro de cada escopo, os
 * símbolos na ordem de declaração. Serve pra imprimir a tabela completa (#25).
 */
void symtab_foreach(void (*fn)(Symbol *sym, void *ctx), void *ctx);

/* Libera todos os símbolos e os escopos. */
void symtab_free(void);

#endif
