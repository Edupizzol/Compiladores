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
./compiler caminho/para/arquivo.c   # roda em cima de um arquivo específico
make clean    # remove os artefatos gerados
```

Os arquivos gerados por bison/flex (`parser.tab.c`, `parser.tab.h`,
`lex.yy.c`) e o binário `compiler` não vão pro repositório (ver
`.gitignore`) — são sempre regenerados pelo `make`.

## Estrutura

```
src/
  lexer.l      # regras léxicas (flex)
  parser.y     # gramática + codegen embutido nas ações (bison)
  types.h      # DataType, ValueData e type_to_rust()
  ast.h        # reservado para uma AST futura (não usado hoje)
  symtab.h     # reservado para tabela de símbolos futura (não usado hoje)
  codegen.h    # reservado para separar o codegen do parser (não usado hoje)
  main.c       # ponto de entrada: abre o arquivo passado em argv e chama yyparse()
tests/inputs/  # arquivos .c de entrada usados para testar o parser
```

`ast.h`, `symtab.h` e `codegen.h` existem como headers vazios: a ideia é
migrar pra eles se o projeto crescer, mas hoje todo o codegen mora direto
nas ações do `parser.y`.

## O que já funciona

- Múltiplas funções por arquivo, com parâmetros tipados e retorno (`int`,
  `float`, `char`).
- Declaração de variável com e sem inicialização, inclusive múltiplas
  variáveis na mesma declaração (`int a, b, c;`).
- Expressões aritméticas, bitwise e lógicas (`+ - * % ^ | & << >> && ||`),
  chamada de função como expressão e comparações — ver branch
  `feat/expr-operadores` (ainda não mergeada na `main`; expande o `expr` que
  hoje só cobre parte dos operadores).
- Comandos de controle de fluxo: `if`/`else` (com dangling-else resolvido
  via `%precedence`), `while`, `for` (as três seções, todas opcionais),
  blocos `{ }` aninhados, `break` e `continue`.
- Mensagem de erro sintático com o número da linha (`yylineno` + `yyerror`).

## Testando

`tests/inputs/exemplo.c` é o alvo do `make run` e cobre o caso geral
(funções, parâmetros, declarações). `tests/inputs/tarefa2_controle_fluxo.c`
cobre especificamente if/else, while, for e break/continue — rode com
`./compiler tests/inputs/tarefa2_controle_fluxo.c`.

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
