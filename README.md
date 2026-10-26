# Compiladores

Trabalho da disciplina de Compiladores: um transpilador de um subconjunto de C
para Rust, construído com **Flex** (análise léxica) e **Bison** (análise
sintática). O parser é dirigido pela sintaxe e, num único passo, faz três
coisas:

- monta a **árvore de derivação** do programa;
- preenche a **tabela de símbolos**, com escopos, e checa os erros
  semânticos;
- gera o **Rust**, montado nas ações das regras e guardado no campo `code` de
  cada nó da árvore.

## Como buildar e rodar

Pré-requisitos: `gcc`, `bison`, `flex` (e `graphviz`, só pra desenhar a
árvore em imagem).

```bash
make          # gera parser.tab.c/h (bison), lex.yy.c (flex) e o binário `compiler`
make run      # roda o compiler em cima de tests/inputs/exemplo.c
make test     # roda a suíte de regressão (tests/expected/) e a sanidade da árvore
make clean    # remove os artefatos gerados

./compiler arquivo.c            # imprime o Rust gerado
./compiler --tokens arquivo.c   # só roda o léxico e lista os tokens (linha, token, texto)
./compiler --tree arquivo.c     # imprime a árvore de derivação em texto
./compiler --dot arquivo.c      # exporta a árvore em Graphviz (DOT)
./compiler --symtab arquivo.c   # imprime a tabela de símbolos
./compiler --help               # lista todas as flags
```

As flags podem ser combinadas, por exemplo `./compiler --tree --symtab
arquivo.c`. A saída sai sempre na ordem tokens, árvore, DOT e tabela. Com
qualquer flag de análise ligada (`--tree`, `--dot`, `--symtab`) o Rust **não**
é impresso, porque a saída passa a ser a análise. O `--tokens` combinado com
uma delas lista os tokens e depois roda o parser no mesmo arquivo.

Exit code: 0 se o programa é válido e ≠ 0 em erro léxico, sintático ou
semântico, ou com arquivo inexistente. Com erro léxico o Rust também não é
impresso, mesmo que o parser consiga seguir.

Os arquivos gerados por bison/flex (`parser.tab.c`, `parser.tab.h`,
`lex.yy.c`) e o binário `compiler` não vão pro repositório (ver
`.gitignore`). O `make` sempre os regenera.

## Estrutura

```
src/
  lexer.l        # regras léxicas (flex); preenche yylloc pra linha de cada token
  parser.y       # gramática; as ações montam a árvore, a tabela e o Rust
  ast.h/.c       # Node da árvore, construção/liberação, tree_print e tree_to_dot
  symtab.h/.c    # tabela de símbolos: hash por nome, pilha e histórico de escopos
  symtab_dump.h/.c # formatação da tabela pro --symtab
  semantic.h/.c  # semantic_error() e a checagem das chamadas de função
  types.h        # DataType, ValueData, type_to_rust() e type_to_c()
  options.h      # flags de CLI (OPT_*) e o switch emit_code
  codegen.h      # reservado para separar o codegen do parser (vazio)
  main.c         # CLI: lê as flags, roda o parser e imprime o que foi pedido
tests/inputs/        # arquivos .c de entrada
tests/expected/      # saída esperada de cada input (.rs, .tree, .dot, .symtab, .err)
tests/run_tests.sh   # runner da suíte de regressão (chamado pelo make test)
tests/check_tree_tokens.sh # sanidade: folhas da árvore x tokens do léxico
```

No `parser.y`, o valor semântico de todo não-terminal é um `Node *`. A ação
de cada regra cria o nó com o helper `mk(label, linha, code, filhos...,
NULL)`. A linha de cada símbolo vem de `@N.first_line` (`%locations`).
`yylineno` não serve, porque pode estar adiantado pelo lookahead.

## Árvore de derivação

É uma árvore de **derivação**, não uma AST enxuta:

- **nó**: um por não-terminal reduzido, com o nome dele na gramática
  (`expr`, `statement`...);
- **folha**: uma por terminal, inclusive a pontuação (`LPAREN`,
  `SEMICOLON`, `','`...). Os tokens com lexema próprio mostram o texto:
  `IDENTIFIER (x)`, `TYPE_KW (int)`, `INT_NUMBER (10)`, `FLOAT_NUMBER (2.0)`;
- **produção vazia**: o nó do não-terminal com um único filho `ε`;
- os filhos ficam na **ordem da regra**, e a recursão à esquerda
  (`function_list`, `statement_list`, `declarator_list`, `param_list`,
  `arg_list`) fica aninhada, sem achatar.

A árvore mostra a precedência e a associatividade de C (`a + b * c` agrupa o
`*`, `a - b - c` associa à esquerda) e o dangling-else (o `ELSE` fica no nó
do `if` mais próximo).

`./compiler --tree` pra `int f(int x) { return x; }`:

```
program
└── function_list
    └── function
        ├── type_specifier
        │   └── TYPE_KW (int)
        ├── IDENTIFIER (f)
        ├── LPAREN
        ├── opt_param_list
        │   └── param_list
        │       └── param
        │           ├── type_specifier
        │           │   └── TYPE_KW (int)
        │           └── IDENTIFIER (x)
        ├── RPAREN
        ├── LBRACE
        ├── statement_list
        │   └── statement
        │       ├── RETURN
        │       ├── expr
        │       │   └── IDENTIFIER (x)
        │       └── SEMICOLON
        └── RBRACE
```

O `--dot` gera um `digraph` com terminais em `box` e não-terminais em
`ellipse`. Pra virar imagem:

```bash
./compiler --dot tests/inputs/exemplo.c | dot -Tpng -o arvore.png
```

Com erro sintático, nenhuma árvore é impressa, nem parcial: sai só a mensagem
de erro.

## Tabela de símbolos

A tabela é preenchida durante o parse por ações no meio das regras. Os
escopos são:

- **global** (nível 0): as funções;
- **função**: parâmetros e corpo dividem o mesmo escopo, como em C;
- **bloco** `{ ... }` (e `{ }`): um escopo novo cada;
- **for**: escopo próprio, então o `i` de `for (int i = 0; ...)` não vaza
  pro resto da função.

Sombrear um nome num escopo interno é válido (`int x; { int x; }`). A função
entra na tabela com a assinatura logo depois do `)`, antes do corpo, então a
recursão funciona.

`./compiler --symtab` pra `int f(int x) { int y = x; return y; }`:

```
ESCOPO  NÍVEL  NOME  CATEGORIA  TIPO       LINHA
0       0      f     função     int (int)  1
1       1      x     parâmetro  int        1
1       1      y     variável   int        1
```

Os escopos já fechados também aparecem. A ordem é por escopo e, dentro de
cada escopo, por declaração. Os tipos saem em C, e as funções com a
assinatura (`int (void)` quando não têm parâmetros).

### Erros semânticos

| erro | mensagem |
| ---- | -------- |
| variável usada ou atribuída sem declaração visível | `variável 'x' não declarada` |
| redeclaração no mesmo escopo | `variável 'x' já declarada neste escopo (linha M)` |
| função definida duas vezes | `função 'f' já definida (linha M)` |
| parâmetro repetido | `parâmetro 'a' repetido (linha M)` |
| chamada de função inexistente | `função 'g' não declarada` |
| número de argumentos errado | `função 'f' espera 1 argumento(s), mas recebeu 2` |
| nome de função usado como variável | `'f' é uma função, não uma variável` |

Todas saem em stderr no formato do erro sintático: `Erro semântico na linha
N: <mensagem>`. Os erros não param o parse, e todos saem no fim, ordenados
por linha. Programa com erro semântico não gera Rust e sai com exit code 1,
mas o `--tree` e o `--symtab` ainda imprimem a árvore e a tabela, o que
ajuda a depurar.

Como a gramática não tem protótipos, as chamadas de função são checadas só
no fim do programa. Assim, chamar uma função definida mais abaixo no arquivo
não é erro (o Rust também aceita funções em qualquer ordem).

## O que já funciona

- Léxico completo: tipos `int`, `float`, `double`, `char`, `void` e
  `struct`; palavras reservadas de controle (`if`, `else`, `while`, `for`,
  `break`, `continue`, `return`); operadores relacionais (`== != < > <= >=`),
  lógicos, bitwise, `++`/`--` e atribuição composta (`+= -= *= /= %= &= |=
  ^= <<= >>=`); `.` e `->`; literais de char e string com escapes
  (`'\0'`, `"a\n"`); comentários `//` e `/* */`; contagem de linha em todas
  as mensagens de erro léxico.
- Múltiplas funções por arquivo, com parâmetros tipados e retorno (`int`,
  `float`, `double`, `char`, `void`).
- Declaração de variável com e sem inicialização, inclusive múltiplas
  variáveis na mesma declaração (`int a, b, c;`).
- Expressões com todos os operadores binários de C (`* / % + - << >> < > <=
  >= == != & ^ | && ||`), unários `-` e `!`, parênteses e chamada de função
  como expressão (`f()`, `f(a, b + 1, g(c))`), com a precedência e a
  associatividade de C. Comparações saem sempre entre parênteses no Rust
  gerado, porque lá elas têm precedência menor que `& ^ |` e não podem ser
  encadeadas.
- Comandos de controle de fluxo: `if`/`else` (com dangling-else resolvido
  via `%precedence`), `while`, `for` (as três seções, todas opcionais),
  blocos `{ }` aninhados, `break` e `continue`.
- Árvore de derivação (`--tree`, `--dot`), tabela de símbolos com escopos
  (`--symtab`) e erros semânticos.
- Mensagens de erro léxico, sintático e semântico com o número da linha.
  Qualquer um deles invalida o programa (exit code ≠ 0, sem Rust).

### Ainda não suportado

- Tokens que o léxico reconhece mas a gramática não usa: `++`/`--`,
  atribuição composta, `.`, `->`, `struct` e literais de char/string.
- Ponteiros e arrays.
- Checagem de tipos das expressões (a tabela guarda os tipos, mas `int x =
  2.5;` não é erro).
- Protótipos de função.

## Testando

`tests/inputs/` tem um assunto por arquivo:

- `exemplo.c` (alvo do `make run`): funções, parâmetros e declarações;
- `tarefa1_expressoes.c`: precedência, unários e chamada de função;
- `tarefa2_controle_fluxo.c`: if/else, while, for e break/continue;
- `lexico.c`: todos os tokens do léxico (rode com `--tokens`). Não compila de
  propósito, porque usa `struct`;
- `arvore_*.c`: os casos da árvore (precedência, dangling-else, `for (;;)`,
  funções);
- `symtab_basico.c` e `simbolos_*.c`: escopos, sombreamento, recursão e
  função definida depois;
- `erro_*.c`: um erro semântico por arquivo.

### A suíte de regressão

`make test` roda a suíte: pra cada arquivo em `tests/expected/`, ele acha o
`tests/inputs/<nome>.c` correspondente, roda o compilador e compara a saída.
Imprime `OK`/`FALHA` por arquivo, mostra o `diff` na falha e sai com código
≠ 0 se algum falhar.

O layout de `tests/expected/` é o seguinte. É a **extensão** do arquivo
esperado que decide a flag passada pro compilador:

| esperado           | flag         | o que é comparado                        |
| ------------------ | ------------ | ---------------------------------------- |
| `<nome>.rs`        | *(nenhuma)*  | stdout: o Rust gerado                    |
| `<nome>.tree`      | `--tree`     | stdout: a árvore de derivação em texto   |
| `<nome>.dot`       | `--dot`      | stdout: a árvore em Graphviz             |
| `<nome>.symtab`    | `--symtab`   | stdout: a tabela de símbolos             |
| `<nome>.err`       | *(nenhuma)*  | stderr, e o exit code tem que ser ≠ 0    |

Pra adicionar um caso de teste:

1. crie `tests/inputs/<nome>.c`;
2. gere o esperado com a flag da extensão, por exemplo
   `./compiler --tree tests/inputs/<nome>.c > tests/expected/<nome>.tree`
   ou `./compiler tests/inputs/<nome>.c 2> tests/expected/<nome>.err`;
3. **revise o arquivo à mão** antes de commitar: um golden file errado vira
   uma regressão "certa".

Não precisa mexer no runner. Os `.rs` são o **baseline do codegen** (issue
#14): travam o Rust gerado pra provar que a árvore e a tabela de símbolos
não o alteraram.

Depois da suíte, o `make test` roda a sanidade da árvore
(`tests/check_tree_tokens.sh`): as folhas da árvore, lidas em ordem, têm que
bater com os tokens do `--tokens`, com a pontuação incluída.

Pra verificar que a gramática não ganhou conflitos ao mexer no `parser.y`,
rode o bison direto com `-Wall`:

```bash
bison -d -Wall -o /tmp/parser.tab.c src/parser.y
```

Qualquer `shift/reduce` ou `reduce/reduce` aparece aí. Os únicos avisos
esperados hoje são os de `IF`/`ELSE`/... redeclarados.

## Acompanhamento

O trabalho está dividido nas issues do repositório:

- [Sprint 4: árvore de derivação (#12)](https://github.com/Edupizzol/Compiladores/issues/12),
  com as sub-issues #14 a #20;
- [Sprint 5: tabela de símbolos e fechamento (#13)](https://github.com/Edupizzol/Compiladores/issues/13),
  com as sub-issues #21 a #28.

As tarefas são divididas por área da gramática pra minimizar conflito de
merge no `parser.y`.
