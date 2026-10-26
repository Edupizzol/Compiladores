#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "options.h"
#include "ast.h"
#include "symtab.h"
#include "semantic.h"
#include "symtab_dump.h"

int yyparse(void);
int yylex(void);
const char *token_name(int tok);
void parser_cleanup(void);
void lexer_free_strings(void);
extern int lexical_errors;
extern FILE *yyin;
extern char *yytext;
extern int yylineno;
extern void yyrestart(FILE *);

/* flags ativas nesta execucao e o switch do codegen (ver options.h) */
int g_options = 0;
int emit_code = 1;

static void usage(FILE *out, const char *prog) {
    fprintf(out,
        "uso: %s [opções] <arquivo.c>\n"
        "\n"
        "opções:\n"
        "  --tokens    só roda o léxico e lista os tokens (linha, token, texto)\n"
        "  --tree      imprime a árvore de derivação em texto\n"
        "  --dot       exporta a árvore de derivação em formato Graphviz\n"
        "  --symtab    imprime a tabela de símbolos\n"
        "  --help      mostra esta ajuda\n"
        "\n"
        "as opções podem ser combinadas: %s --tree --dot arquivo.c\n",
        prog, prog);
}

/* modo --tokens: so roda o lexico e lista os tokens, sem passar pelo parser */
static int dump_tokens(void) {
    int tok;
    while ((tok = yylex()) != 0)
        printf("%4d  %-16s %s\n", yylineno, token_name(tok), yytext);
    return 0;
}

int main(int argc, char **argv) {
    const char *prog = argc > 0 ? argv[0] : "compiler";
    const char *input = NULL;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--tokens") == 0) {
            g_options |= OPT_TOKENS;
        } else if (strcmp(arg, "--tree") == 0) {
            g_options |= OPT_TREE;
        } else if (strcmp(arg, "--dot") == 0) {
            g_options |= OPT_DOT;
        } else if (strcmp(arg, "--symtab") == 0) {
            g_options |= OPT_SYMTAB;
        } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            usage(stdout, prog);
            return 0;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            fprintf(stderr, "opção inválida: %s\n\n", arg);
            usage(stderr, prog);
            return 1;
        } else if (input != NULL) {
            fprintf(stderr, "mais de um arquivo de entrada: %s e %s\n\n", input, arg);
            usage(stderr, prog);
            return 1;
        } else {
            input = arg;
        }
    }

    if (input == NULL) {
        usage(stderr, prog);
        return 1;
    }

    /* o Rust so e impresso se nenhuma flag de analise estiver ligada */
    emit_code = (g_options & (OPT_TREE | OPT_DOT | OPT_SYMTAB)) == 0;

    yyin = fopen(input, "r");
    if (!yyin) {
        perror("fopen");
        return 1;
    }

    /* escopo global da tabela de simbolos, preenchida durante o parse */
    symtab_init();

    int ret;
    if (g_options & OPT_TOKENS) {
        ret = dump_tokens();
        if (g_options & (OPT_TREE | OPT_DOT | OPT_SYMTAB)) {
            /* combinado com uma flag de analise: volta pro comeco do arquivo
               pra que o parser tambem possa rodar em cima dele */
            rewind(yyin);
            yyrestart(yyin);
            yylineno = 1;
            ret = yyparse();
        }
    } else {
        ret = yyparse();
    }

    fclose(yyin);

    /* com erro sintatico nao ha arvore nem tabela: so a mensagem do yyerror */
    int syntax_ok = ret == 0;

    /* os erros semanticos saem todos juntos, em ordem de linha. Nao
       interrompem o parse, mas o programa e invalido: exit code != 0 */
    semantic_flush();
    if (syntax_ok && semantic_errors > 0)
        ret = 1;
    /* erro lexico tambem invalida o programa, mesmo no --tokens */
    if (lexical_errors > 0)
        ret = 1;

    /* a arvore e a tabela saem mesmo com erro semantico (ajuda a depurar) */
    if (syntax_ok && (g_options & OPT_TREE))
        tree_print(parse_root, stdout);
    if (syntax_ok && (g_options & OPT_DOT))
        tree_to_dot(parse_root, stdout);
    if (syntax_ok && (g_options & OPT_SYMTAB))
        symtab_dump(stdout);

    node_free(parse_root);
    parser_cleanup();
    symtab_free();
    lexer_free_strings();
    return ret;
}
