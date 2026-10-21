# Compiladores

Trabalho da disciplina de Compiladores: um transpilador de um subconjunto de C
para Rust, construído com **Flex** (análise léxica) e **Bison** (análise
sintática dirigida por sintaxe — o próprio parser já gera o código de saída
nas ações das regras, sem passar por uma AST intermediária).

## Como buildar e rodar

Pré-requisitos: `gcc`, `bison`, `flex`.

```bash
make          # gera parser.tab.c/h (bison), lex.yy.c (flex) e o binário `compiler`
make run      # roda o compiler em cima de tests/inputs/exemplo.c
make test     # roda a suíte de regressão (tests/expected/)
./compiler caminho/para/arquivo.c   # roda em cima de um arquivo específico
./compiler --tokens arquivo.c       # só roda o léxico e lista os tokens (linha, token, texto)
./compiler --help                   # lista todas as flags
make clean    # remove os artefatos gerados
```

As flags de CLI são combináveis entre si. Com qualquer flag de análise
ligada (`--tree`, `--dot`, `--symtab`) o Rust gerado **não** é impresso — a
saída passa a ser a análise.

Os arquivos gerados por bison/flex (`parser.tab.c`, `parser.tab.h`,
`lex.yy.c`) e o binário `compiler` não vão pro repositório (ver
`.gitignore`) — são sempre regenerados pelo `make`.

## Estrutura

```
src/
  lexer.l      # regras léxicas (flex)
  parser.y     # gramática + codegen embutido nas ações (bison)
  types.h      # DataType, ValueData e type_to_rust()
  options.h    # flags de CLI e o switch emit_code
  ast.h        # reservado para uma AST futura (não usado hoje)
  symtab.h     # reservado para tabela de símbolos futura (não usado hoje)
  codegen.h    # reservado para separar o codegen do parser (não usado hoje)
  main.c       # ponto de entrada: abre o arquivo passado em argv e chama yyparse()
tests/inputs/   # arquivos .c de entrada usados para testar o parser
tests/expected/ # arquivos .rs/.tree/.symtab/.err com a saída esperada de cada input
tests/run_tests.sh # runner da suíte de regressão (chamado pelo make test)
```

`ast.h`, `symtab.h` e `codegen.h` existem como headers vazios: a ideia é
migrar pra eles se o projeto crescer, mas hoje todo o codegen mora direto
nas ações do `parser.y`.

## O que já funciona

- Léxico completo: tipos `int`, `float`, `double`, `char`, `void` e
  `struct`; palavras reservadas de controle (`if`, `else`, `while`, `for`,
  `break`, `continue`, `return`); operadores relacionais (`== != < > <= >=`),
  lógicos, bitwise, `++`/`--` e atribuição composta (`+= -= *= /= %= &= |=
  ^= <<= >>=`); `.` e `->`; literais de char e string com escapes
  (`'\0'`, `"a\n"`); comentários `//` e `/* */`; contagem de linha em todas
  as mensagens de erro léxico. Nem todos esses tokens têm regra na gramática
  ainda — ver as issues de cada tarefa.
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
- Mensagem de erro sintático com o número da linha (`yylineno` + `yyerror`).

## Testando

`tests/inputs/exemplo.c` é o alvo do `make run` e cobre o caso geral
(funções, parâmetros, declarações). `tests/inputs/tarefa2_controle_fluxo.c`
cobre especificamente if/else, while, for e break/continue — rode com
`./compiler tests/inputs/tarefa2_controle_fluxo.c`.
`tests/inputs/tarefa1_expressoes.c` cobre precedência, unários e chamada de
função — rode com `./compiler tests/inputs/tarefa1_expressoes.c`.
`tests/inputs/lexico.c` passa por todos os tokens do léxico — rode com
`./compiler --tokens tests/inputs/lexico.c` e confira os tokens e as linhas.

### A suíte de regressão

`make test` roda a suíte: pra cada arquivo em `tests/expected/`, ele acha o
`tests/inputs/<nome>.c` correspondente, roda o compilador e compara a saída.
Imprime `OK`/`FALHA` por arquivo, mostra o `diff` na falha e sai com código
≠ 0 se algum falhar.

O layout de `tests/expected/` é o seguinte — a **extensão** do arquivo
esperado é que decide a flag passada pro compilador:

| esperado           | flag         | o que é comparado                        |
| ------------------ | ------------ | ---------------------------------------- |
| `<nome>.rs`        | *(nenhuma)*  | stdout — o Rust gerado                   |
| `<nome>.tree`      | `--tree`     | stdout — a árvore de derivação em texto  |
| `<nome>.symtab`    | `--symtab`   | stdout — a tabela de símbolos            |
| `<nome>.err`       | *(nenhuma)*  | stderr, e o exit code tem que ser ≠ 0   |

Ou seja: pra adicionar um caso de teste é só soltar o arquivo esperado em
`tests/expected/` — não precisa mexer no runner. Os `.rs` já existentes são
o **baseline do codegen** (issue #14): travam a saída atual do compilador
pra provar que as mudanças na árvore e na tabela de símbolos não alteraram
o Rust gerado.

Pra verificar se a gramática não introduziu conflitos ao mexer no
`parser.y`, rode o bison direto com `-Wall`:

```bash
bison -d -Wall -o /tmp/parser.tab.c src/parser.y
```

Qualquer `shift/reduce` ou `reduce/reduce` aparece aí.

## Acompanhamento

O trabalho está dividido por área da gramática nas issues do repositório
(veja a [#3](https://github.com/Edupizzol/Compiladores/issues/3) e as
sub-issues de cada tarefa), pra minimizar conflito de merge no
`parser.y` — cada tarefa mexe num bloco diferente do arquivo.
