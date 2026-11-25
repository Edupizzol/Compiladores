%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "types.h"
#include "options.h"
#include "ast.h"
#include "symtab.h"
#include "semantic.h"
#include "optimize.h"

void yyerror(const char *s);
int yylex(void);

/* cria um no `label` e anexa os filhos dados (lista terminada em NULL); o
   no passa a ser dono deles. O Rust nao e montado aqui: ele sai depois,
   da arvore inteira, pelo codegen_rust (#54) */
static Node *mk(const char *label, int line, ...) {
    Node *n = node_new(label, line);
    va_list args;
    va_start(args, line);
    for (Node *c; (c = va_arg(args, Node *)) != NULL; )
        node_add_child(n, c);
    va_end(args);
    return n;
}

/* folha de um token de texto fixo (pontuacao, operador, palavra reservada):
   so o nome do token, sem lexema. Os tokens com lexema proprio (IDENTIFIER,
   TYPE_KW, numeros) usam node_leaf direto */
static Node *tok(const char *label, int line) {
    return node_leaf(label, NULL, line);
}

/* tipo da declaracao em andamento, setado pela acao intermediaria em
   `type_specifier` antes de reduzir declarator_list (statement e for_init) */
static DataType current_decl_type;

/* assinatura da funcao em andamento: o tipo de retorno e guardado logo
   depois do LPAREN (antes que os params troquem o current_decl_type), e cada
   `param` acrescenta o seu tipo. A funcao entra na tabela depois do RPAREN */
static DataType current_func_ret;
static const char *current_func_name;
static DataType *current_func_params;
static int current_func_n, current_func_cap;

/* uso de `name` como variavel (em expressao ou como alvo de atribuicao):
   tem que haver uma variavel ou parametro visivel com esse nome */
static DataType check_var_use(const char *name, int line) {
    Symbol *sym = symtab_lookup(name);
    if (!sym) {
        semantic_error(line, "variável '%s' não declarada", name);
        return TYPE_ERROR;
    }
    if (sym->kind == SYM_FUNC) {
        semantic_error(line, "'%s' é uma função, não uma variável", name);
        return TYPE_ERROR;
    }
    /* variavel void ja foi reportada na declaracao */
    return sym->type == TYPE_VOID ? TYPE_ERROR : sym->type;
}

/* quantos argumentos tem um opt_arg_list: o vazio tem so o filho ε, e o
   arg_list aninha a esquerda (arg_list ',' expr) */
static int count_args(const Node *opt_arg_list) {
    const Node *list = opt_arg_list->children[0];
    if (strcmp(list->label, "arg_list") != 0) return 0;
    int n = 1;
    while (list->n_children == 3) {
        n++;
        list = list->children[0];
    }
    return n;
}

/* declaracao de variavel: redeclarar no mesmo escopo e erro, mas esconder
   um nome de um escopo externo (sombreamento) e valido */
static void declare_var(const char *name, int line) {
    if (current_decl_type == TYPE_VOID)
        semantic_error(line, "variável '%s' não pode ser do tipo void", name);
    if (symtab_insert(name, SYM_VAR, current_decl_type, line) != 0)
        semantic_error(line, "variável '%s' já declarada neste escopo (linha %d)",
                       name, symtab_lookup_current(name)->line);
}

static void func_param_add(DataType t) {
    if (current_func_n == current_func_cap) {
        current_func_cap = current_func_cap ? current_func_cap * 2 : 4;
        current_func_params = realloc(current_func_params, current_func_cap * sizeof(DataType));
    }
    current_func_params[current_func_n++] = t;
}

/* com -O, dobra a expressao recem-criada se os operandos forem literais
   (#49). Os filhos ja passaram por aqui quando foram reduzidos */
static Node *opt_expr(Node *n) {
    if (g_options & OPT_OPTIMIZE) {
        fold_constants(n);
        simplify_algebraic(n);    /* pega os casos com variavel (#50) */
    }
    return n;
}

/* ---- tipos (#44) ---- */

/* tipo "silencioso": erro ja reportado ou ainda desconhecido, nao gera
   outro erro por cima */
static int type_silent(DataType t) {
    return t == TYPE_ERROR || t == TYPE_UNKNOWN || t == TYPE_NONE;
}

/* `src` guardado num destino do tipo `dst` (atribuicao, inicializacao,
   return): void nao e valor. `fmt` e `name` montam a mensagem do erro. A
   conversao (o cast no Rust) fica com o codegen */
static void check_value(const Node *src, DataType dst, int line, const char *fmt, const char *name) {
    if (src->type == TYPE_VOID && dst != TYPE_ERROR)
        semantic_error(line, fmt, name);
}

/* condicao de if/while/for tem que ser um valor */
static void check_cond(const Node *cond, int line, const char *what) {
    if (cond->type == TYPE_VOID)
        semantic_error(line, "a condição do '%s' é void", what);
}

/* tipos de operador: aritmetico, so inteiro (% << >> & | ^), comparacao e
   logico (&& ||) */
enum { OPK_ARITH, OPK_INT, OPK_CMP, OPK_LOGIC };

/* no de expressao binaria `expr OP expr`: o operador vira folha do token
   (PLUS, STAR...) entre os dois operandos. Calcula o tipo do resultado, com
   as conversoes aritmeticas de C (comparacao e logico dao int, como em C) */
static Node *binop_node(Node *lhs, const char *op_tok, const char *op, int op_line, Node *rhs, int kind) {
    DataType lt = lhs->type, rt = rhs->type;
    DataType res = TYPE_ERROR;

    if (lt == TYPE_VOID || rt == TYPE_VOID) {
        semantic_error(op_line, "operador '%s' não se aplica a operando void", op);
    } else if (lt == TYPE_ERROR || rt == TYPE_ERROR) {
        res = TYPE_ERROR;
    } else if (lt == TYPE_UNKNOWN || rt == TYPE_UNKNOWN) {
        res = TYPE_UNKNOWN;
    } else if (kind == OPK_INT && !(type_is_integer(lt) && type_is_integer(rt))) {
        semantic_error(op_line, "operador '%s' exige operandos inteiros (recebeu %s e %s)",
                       op, type_to_c(lt), type_to_c(rt));
    } else {
        res = (kind == OPK_ARITH || kind == OPK_INT) ? type_promote(lt, rt) : TYPE_INT;
    }

    Node *n = mk("expr", lhs->line, lhs, tok(op_tok, op_line), rhs, NULL);
    n->type = res;
    return opt_expr(n);
}

/* escreve em out[0..n) os nos dos argumentos de um opt_arg_list com n
   argumentos (arg_list aninha a esquerda: o ultimo e o filho da direita) */
static void collect_args(const Node *opt_arg_list, const Node **out, int n) {
    const Node *list = opt_arg_list->children[0];
    for (int i = n - 1; i >= 0; i--) {
        out[i] = list->children[list->n_children - 1];
        list = list->children[0];
    }
}
/* tipo de um operador unario: void nao vale; `fixed` e o tipo do resultado
   (`!` da sempre int), ou TYPE_NONE pra promover o do operando (`-` de char
   da int) */
static DataType unary_type(const Node *operand, const char *op, int line, DataType fixed) {
    DataType t = operand->type;
    if (t == TYPE_VOID) {
        semantic_error(line, "operador '%s' não se aplica a operando void", op);
        return TYPE_ERROR;
    }
    if (type_silent(t)) return t == TYPE_NONE ? TYPE_ERROR : t;
    return fixed != TYPE_NONE ? fixed : type_promote(t, TYPE_INT);
}
%}

/* o Node da %union precisa estar visivel em parser.tab.h (lexer e main) */
%code requires {
#include "ast.h"
}

/* no erro sintatico o bison descarta os nos que ja estavam na pilha: sem
   isto eles vazariam (a arvore parcial nunca chega em parse_root) */
%destructor { node_free($$); } <node>

%union {
    DataType data_type;
    ValueData val;
    Node *node;
}

/* @N.first_line da a linha de cada simbolo (o lexer preenche yylloc em
   YY_USER_ACTION); yylineno na acao pode estar adiantado pelo lookahead */
%locations

/* expoe yytname pra token_name() (usado pelo modo --tokens do main.c) */
%token-table

%token <val> IDENTIFIER INT_NUMBER FLOAT_NUMBER
%token <data_type> TYPE_KW
%token RETURN ASSIGN SEMICOLON LBRACE RBRACE LPAREN RPAREN
%token PLUS MINUS STAR MOD XOR OR BINOR AND EC SHIFTL SHIFTR COMP
%token IF ELSE WHILE FOR BREAK CONTINUE

/* tokens do lexico completo (issue #2) - a gramatica ainda nao usa todos */
%token <val> CHAR_LITERAL STRING_LITERAL
%token STRUCT IF ELSE WHILE FOR BREAK CONTINUE
%token NE LT GT LE GE NOT SLASH DOT ARROW
%token INC DEC ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN
%token AND_ASSIGN OR_ASSIGN XOR_ASSIGN SHL_ASSIGN SHR_ASSIGN

%type <node> statement_list statement expr function_list function opt_param_list param_list param
%type <node> declarator declarator_list for_init for_cond for_incr
%type <node> opt_arg_list arg_list type_specifier

/* precedencia e associatividade de C, da menor pra maior */
%left OR
%left AND
%left BINOR
%left XOR
%left EC
%left COMP NE
%left LT GT LE GE
%left SHIFTL SHIFTR
%left PLUS MINUS
%left STAR SLASH MOD
%precedence NOT UMINUS

/* dangling-else: favorece o shift do ELSE, amarrando ao if mais próximo */
%precedence IFX
%precedence ELSE

%%

/* o escopo global e criado pelo symtab_init() do main, antes do parse, e
   nao fecha (scope_exit no global nao faz nada) */
program:
    function_list {
        parse_root = mk("program", @1.first_line, $1, NULL);
        /* com o programa inteiro lido, todas as funcoes ja estao na tabela */
        semantic_check_calls();
        semantic_resolve_types(parse_root);
        /* o Rust sai depois, pelo codegen_rust no main */
    }
;

function_list:
    function {
        $$ = mk("function_list", @1.first_line, $1, NULL);
    }
    | function_list function {
        $$ = mk("function_list", @1.first_line, $1, $2, NULL);
    }
;

/* parametros e corpo dividem o mesmo escopo (como em C): abre logo depois
   do LPAREN e fecha depois do RBRACE. A funcao entra no escopo global logo
   depois do RPAREN, ja com a assinatura e antes do corpo (pra recursao).
   As acoes do meio ocupam o $4 e o $7, entao opt_param_list e o $5 e
   statement_list o $9 */
function:
    type_specifier IDENTIFIER LPAREN {
        scope_enter();
        current_func_ret = current_decl_type;
        current_func_name = $2.s_val;
        current_func_n = 0;
    }
    opt_param_list RPAREN {
        if (symtab_insert_func($2.s_val, current_func_ret, current_func_n,
                               current_func_params, @2.first_line) != 0) {
            semantic_error(@2.first_line, "função '%s' já definida (linha %d)",
                           $2.s_val, symtab_lookup_func($2.s_val)->line);
        }
    }
    LBRACE statement_list RBRACE {
        scope_exit();
        $$ = mk("function", @1.first_line,
                $1, node_leaf("IDENTIFIER", $2.s_val, @2.first_line), tok("LPAREN", @3.first_line),
                $5, tok("RPAREN", @6.first_line), tok("LBRACE", @8.first_line),
                $9, tok("RBRACE", @10.first_line), NULL);
    }
;

opt_param_list:
    param_list {
        $$ = mk("opt_param_list", @1.first_line, $1, NULL);
    }
    | /* vazio */ {
        $$ = mk("opt_param_list", @$.first_line, node_leaf("ε", NULL, @$.first_line), NULL);
    }
;

param_list:
    param {
        $$ = mk("param_list", @1.first_line, $1, NULL);
    }
    | param_list ',' param {
        $$ = mk("param_list", @1.first_line, $1, tok("','", @2.first_line), $3, NULL);
    }
;

param:
    type_specifier IDENTIFIER {
        /* o type_specifier acabou de ser reduzido: current_decl_type e o tipo dele */
        if (current_decl_type == TYPE_VOID)
            semantic_error(@2.first_line, "parâmetro '%s' não pode ser do tipo void", $2.s_val);
        if (symtab_insert($2.s_val, SYM_PARAM, current_decl_type, @2.first_line) != 0) {
            semantic_error(@2.first_line, "parâmetro '%s' repetido (linha %d)",
                           $2.s_val, symtab_lookup_current($2.s_val)->line);
        }
        /* o repetido conta na aridade mesmo assim: f(int a, int a) recebe 2 */
        func_param_add(current_decl_type);
        $$ = mk("param", @1.first_line, $1, node_leaf("IDENTIFIER", $2.s_val, @2.first_line), NULL);
    }
;

type_specifier:
    TYPE_KW {
        /* reduzido antes de declarator_list, entao ja vale pra declaracao */
        current_decl_type = $1;
        $$ = mk("type_specifier", @1.first_line,
                node_leaf("TYPE_KW", type_to_c($1), @1.first_line), NULL);
    }
;

statement_list:
    statement {
        $$ = mk("statement_list", @1.first_line, $1, NULL);
    }
    | statement_list statement {
        $$ = mk("statement_list", @1.first_line, $1, $2, NULL);
    }
;

statement:
    type_specifier declarator_list SEMICOLON {
        $$ = mk("statement", @1.first_line,
                $1, $2, tok("SEMICOLON", @3.first_line), NULL);
    }
    | IDENTIFIER ASSIGN expr SEMICOLON {
        DataType target = check_var_use($1.s_val, @1.first_line);
        check_value($3, target, @2.first_line, "atribuição de valor void a '%s'", $1.s_val);
        $$ = mk("statement", @1.first_line,
                node_leaf("IDENTIFIER", $1.s_val, @1.first_line), tok("ASSIGN", @2.first_line),
                $3, tok("SEMICOLON", @4.first_line), NULL);
    }
    | RETURN expr SEMICOLON {
        if (current_func_ret == TYPE_VOID) {
            if ($2->type != TYPE_ERROR)
                semantic_error(@1.first_line, "função '%s' é void e não pode retornar valor", current_func_name);
        } else {
            check_value($2, current_func_ret, @1.first_line, "'return' com valor void em '%s'", current_func_name);
        }
        $$ = mk("statement", @1.first_line,
                tok("RETURN", @1.first_line), $2, tok("SEMICOLON", @3.first_line), NULL);
    }
    | IF LPAREN expr RPAREN statement %prec IFX {
        check_cond($3, @1.first_line, "if");
        $$ = mk("statement", @1.first_line,
                tok("IF", @1.first_line), tok("LPAREN", @2.first_line), $3,
                tok("RPAREN", @4.first_line), $5, NULL);
    }
    | IF LPAREN expr RPAREN statement ELSE statement {
        /* o ELSE fica no mesmo no do IF mais proximo (dangling-else) */
        check_cond($3, @1.first_line, "if");
        $$ = mk("statement", @1.first_line,
                tok("IF", @1.first_line), tok("LPAREN", @2.first_line), $3,
                tok("RPAREN", @4.first_line), $5, tok("ELSE", @6.first_line), $7, NULL);
    }
    | WHILE LPAREN expr RPAREN statement {
        check_cond($3, @1.first_line, "while");
        $$ = mk("statement", @1.first_line,
                tok("WHILE", @1.first_line), tok("LPAREN", @2.first_line), $3,
                tok("RPAREN", @4.first_line), $5, NULL);
    }
    /* o for tem escopo proprio, aberto antes do for_init, pra que o `i` de
       `for (int i = 0; ...)` nao vaze pro resto da funcao. A acao do meio
       ocupa o $3 */
    | FOR LPAREN { scope_enter(); } for_init SEMICOLON for_cond SEMICOLON for_incr RPAREN statement {
        scope_exit();
        $$ = mk("statement", @1.first_line,
                tok("FOR", @1.first_line), tok("LPAREN", @2.first_line), $4,
                tok("SEMICOLON", @5.first_line), $6, tok("SEMICOLON", @7.first_line), $8,
                tok("RPAREN", @9.first_line), $10, NULL);
    }
    /* cada bloco abre um escopo; a acao do meio ocupa o $2 */
    | LBRACE { scope_enter(); } statement_list RBRACE {
        scope_exit();
        $$ = mk("statement", @1.first_line,
                tok("LBRACE", @1.first_line), $3, tok("RBRACE", @4.first_line), NULL);
    }
    | LBRACE RBRACE {
        /* bloco vazio tambem conta como escopo (sem simbolos) */
        scope_enter();
        scope_exit();
        $$ = mk("statement", @1.first_line,
                tok("LBRACE", @1.first_line), tok("RBRACE", @2.first_line), NULL);
    }
    | BREAK SEMICOLON {
        $$ = mk("statement", @1.first_line,
                tok("BREAK", @1.first_line), tok("SEMICOLON", @2.first_line), NULL);
    }
    | CONTINUE SEMICOLON {
        $$ = mk("statement", @1.first_line,
                tok("CONTINUE", @1.first_line), tok("SEMICOLON", @2.first_line), NULL);
    }
;

/* as tres secoes do for, cada uma opcional: for (init; cond; incr).
   Secao vazia vira o no da secao com um filho ε */
for_init:
    /* vazio */ {
        $$ = mk("for_init", @$.first_line, node_leaf("ε", NULL, @$.first_line), NULL);
    }
    | type_specifier declarator_list {
        $$ = mk("for_init", @1.first_line, $1, $2, NULL);
    }
    | IDENTIFIER ASSIGN expr {
        DataType target = check_var_use($1.s_val, @1.first_line);
        check_value($3, target, @2.first_line, "atribuição de valor void a '%s'", $1.s_val);
        $$ = mk("for_init", @1.first_line,
                node_leaf("IDENTIFIER", $1.s_val, @1.first_line), tok("ASSIGN", @2.first_line), $3, NULL);
    }
;

for_cond:
    /* vazio */ {
        $$ = mk("for_cond", @$.first_line, node_leaf("ε", NULL, @$.first_line), NULL);
    }
    | expr {
        check_cond($1, @1.first_line, "for");
        $$ = mk("for_cond", @1.first_line, $1, NULL);
    }
;

for_incr:
    /* vazio */ {
        $$ = mk("for_incr", @$.first_line, node_leaf("ε", NULL, @$.first_line), NULL);
    }
    | IDENTIFIER ASSIGN expr {
        DataType target = check_var_use($1.s_val, @1.first_line);
        check_value($3, target, @2.first_line, "atribuição de valor void a '%s'", $1.s_val);
        $$ = mk("for_incr", @1.first_line,
                node_leaf("IDENTIFIER", $1.s_val, @1.first_line), tok("ASSIGN", @2.first_line), $3, NULL);
    }
;

/* uma variavel dentro de uma declaracao, com ou sem inicializacao:
   `int x;` ou `int x = 5;` (usa current_decl_type pro tipo) */
declarator:
    IDENTIFIER {
        declare_var($1.s_val, @1.first_line);
        $$ = mk("declarator", @1.first_line,
                node_leaf("IDENTIFIER", $1.s_val, @1.first_line), NULL);
    }
    | IDENTIFIER ASSIGN expr {
        declare_var($1.s_val, @1.first_line);
        /* o tipo declarado vale pro valor: int x = 2.5 converte (o cast fica
           com o codegen) */
        check_value($3, current_decl_type, @2.first_line, "inicialização de '%s' com valor void", $1.s_val);
        $$ = mk("declarator", @1.first_line,
                node_leaf("IDENTIFIER", $1.s_val, @1.first_line), tok("ASSIGN", @2.first_line), $3, NULL);
    }
;

/* recursiva a esquerda: `int a, b, c;` fica aninhada, sem achatar */
declarator_list:
    declarator {
        $$ = mk("declarator_list", @1.first_line, $1, NULL);
    }
    | declarator_list ',' declarator {
        $$ = mk("declarator_list", @1.first_line,
                $1, tok("','", @2.first_line), $3, NULL);
    }
;

expr:
    INT_NUMBER {
        $$ = mk("expr", @1.first_line, node_leaf("INT_NUMBER", $1.s_val, @1.first_line), NULL);
        $$->type = TYPE_INT;
    }
    | FLOAT_NUMBER {
        $$ = mk("expr", @1.first_line, node_leaf("FLOAT_NUMBER", $1.s_val, @1.first_line), NULL);
        $$->type = TYPE_DOUBLE; /* literal sem sufixo e double em C */
    }
    | IDENTIFIER {
        DataType t = check_var_use($1.s_val, @1.first_line);
        $$ = mk("expr", @1.first_line, node_leaf("IDENTIFIER", $1.s_val, @1.first_line), NULL);
        $$->type = t;
    }
    | LPAREN expr RPAREN {
        /* a arvore guarda os parenteses como estao no fonte */
        $$ = mk("expr", @1.first_line,
                tok("LPAREN", @1.first_line), $2, tok("RPAREN", @3.first_line), NULL);
        $$->type = $2->type;
        opt_expr($$);
    }
    | IDENTIFIER LPAREN opt_arg_list RPAREN {
        /* a aridade (e a existencia) so e checada no fim do programa: a
           funcao pode vir depois */
        int n = count_args($3);
        semantic_call($1.s_val, n, @1.first_line);
        const Node **args = malloc((n + 1) * sizeof(Node *));
        collect_args($3, args, n);
        Symbol *f = symtab_lookup_func($1.s_val);
        DataType rtype = TYPE_UNKNOWN;
        if (f) {
            /* funcao ja definida: o resultado tem o tipo de retorno dela (os
               casts dos argumentos pros tipos dos parametros ficam com o
               codegen) */
            rtype = f->type;
            for (int i = 0; i < n; i++)
                if (args[i]->type == TYPE_VOID)
                    semantic_error(@1.first_line, "argumento %d de '%s' é void", i + 1, $1.s_val);
        } else {
            /* funcao definida mais adiante: tipo do resultado desconhecido, e
               os argumentos so podem ser checados no fim do programa */
            DataType *types = malloc((n + 1) * sizeof(DataType));
            for (int i = 0; i < n; i++) types[i] = args[i]->type;
            semantic_call_arg_types(types, n);
            free(types);
        }
        free(args);
        $$ = mk("expr", @1.first_line,
                node_leaf("IDENTIFIER", $1.s_val, @1.first_line), tok("LPAREN", @2.first_line),
                $3, tok("RPAREN", @4.first_line), NULL);
        $$->type = rtype;
    }
    | expr PLUS expr    { $$ = binop_node($1, "PLUS", "+", @2.first_line, $3, OPK_ARITH); }
    | expr MINUS expr   { $$ = binop_node($1, "MINUS", "-", @2.first_line, $3, OPK_ARITH); }
    | expr STAR expr    { $$ = binop_node($1, "STAR", "*", @2.first_line, $3, OPK_ARITH); }
    | expr SLASH expr   { $$ = binop_node($1, "SLASH", "/", @2.first_line, $3, OPK_ARITH); }
    | expr MOD expr     { $$ = binop_node($1, "MOD", "%", @2.first_line, $3, OPK_INT); }
    | expr XOR expr     { $$ = binop_node($1, "XOR", "^", @2.first_line, $3, OPK_INT); }
    | expr OR expr      { $$ = binop_node($1, "OR", "||", @2.first_line, $3, OPK_LOGIC); }
    | expr BINOR expr   { $$ = binop_node($1, "BINOR", "|", @2.first_line, $3, OPK_INT); }
    | expr AND expr     { $$ = binop_node($1, "AND", "&&", @2.first_line, $3, OPK_LOGIC); }
    | expr EC expr      { $$ = binop_node($1, "EC", "&", @2.first_line, $3, OPK_INT); }
    | expr SHIFTL expr  { $$ = binop_node($1, "SHIFTL", "<<", @2.first_line, $3, OPK_INT); }
    | expr SHIFTR expr  { $$ = binop_node($1, "SHIFTR", ">>", @2.first_line, $3, OPK_INT); }
    | expr COMP expr    { $$ = binop_node($1, "COMP", "==", @2.first_line, $3, OPK_CMP); }
    | expr NE expr      { $$ = binop_node($1, "NE", "!=", @2.first_line, $3, OPK_CMP); }
    | expr LT expr      { $$ = binop_node($1, "LT", "<", @2.first_line, $3, OPK_CMP); }
    | expr GT expr      { $$ = binop_node($1, "GT", ">", @2.first_line, $3, OPK_CMP); }
    | expr LE expr      { $$ = binop_node($1, "LE", "<=", @2.first_line, $3, OPK_CMP); }
    | expr GE expr      { $$ = binop_node($1, "GE", ">=", @2.first_line, $3, OPK_CMP); }
    | NOT expr {
        $$ = mk("expr", @1.first_line, tok("NOT", @1.first_line), $2, NULL);
        $$->type = unary_type($2, "!", @1.first_line, TYPE_INT);
        opt_expr($$);
    }
    | MINUS expr %prec UMINUS {
        DataType t = unary_type($2, "-", @1.first_line, TYPE_NONE);
        $$ = mk("expr", @1.first_line, tok("MINUS", @1.first_line), $2, NULL);
        $$->type = t;
        opt_expr($$);
    }
;

/* argumentos de uma chamada: f(), f(a) ou f(a, b + 1, g(c)) */
opt_arg_list:
    arg_list { $$ = mk("opt_arg_list", @1.first_line, $1, NULL); }
    | /* vazio */ {
        $$ = mk("opt_arg_list", @$.first_line, node_leaf("ε", NULL, @$.first_line), NULL);
    }
;

arg_list:
    expr { $$ = mk("arg_list", @1.first_line, $1, NULL); }
    | arg_list ',' expr {
        $$ = mk("arg_list", @1.first_line, $1, tok("','", @2.first_line), $3, NULL);
    }
;

%%

extern int yylineno;
extern char *yytext;

void yyerror(const char *s) {
    /* no EOF o yytext vem vazio: "proximo a ''" nao diz nada */
    if (yytext[0] == '\0')
        fprintf(stderr, "Erro sintático na linha %d: %s (no fim do arquivo)\n", yylineno, s);
    else
        fprintf(stderr, "Erro sintático na linha %d: %s (próximo a '%s')\n", yylineno, s, yytext);
}

/* libera o que o parser guarda entre as regras (chamada pelo main no fim) */
void parser_cleanup(void) {
    free(current_func_params);
    current_func_params = NULL;
    current_func_n = current_func_cap = 0;
}

const char *token_name(int tok) {
    return yytname[YYTRANSLATE(tok)];
}
