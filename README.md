# Compiladores

Trabalho da disciplina de Compiladores: um transpilador de um subconjunto de C
para Rust, construído com **Flex** (análise léxica) e **Bison** (análise
sintática). O fluxo é:

1. o parser monta a **árvore de derivação**, preenche a **tabela de
   símbolos** com escopos, anota o **tipo** de cada expressão e reporta os
   erros semânticos;
2. com `-O`, a árvore passa pelos passes de otimização (`src/optimize.c`,
   `src/tac_opt.c`) durante o próprio parse;
3. o gerador de código (`src/codegen.c`) percorre a árvore anotada e imprime
   o **Rust**. O TAC (`src/tac.c`) e o bytecode de máquina de pilha
   (`src/bytecode.c`) saem da mesma árvore, pelas suas flags.

## Como buildar e rodar

Pré-requisitos: `gcc`, `bison`, `flex` (e `graphviz`, só pra desenhar a
árvore em imagem).

```bash
make          # gera parser.tab.c/h (bison), lex.yy.c (flex) e o binário `compiler`
make run      # roda o compiler em cima de tests/inputs/exemplo.c
make test     # roda a suíte de regressão (tests/expected/) e a sanidade da árvore
make asan     # builda com ASan/UBSan e roda todos os inputs, com e sem -O
make clean    # remove os artefatos gerados

./compiler arquivo.c            # imprime o Rust gerado
./compiler --tokens arquivo.c   # só roda o léxico e lista os tokens (linha, token, texto)
./compiler --tree arquivo.c     # imprime a árvore de derivação em texto
./compiler --types arquivo.c    # imprime a árvore com o tipo de cada expressão
./compiler --tac arquivo.c      # imprime o código de três endereços (TAC)
./compiler --bytecode arquivo.c # imprime o bytecode de máquina de pilha
./compiler --dot arquivo.c      # exporta a árvore em Graphviz (DOT)
./compiler --symtab arquivo.c   # imprime a tabela de símbolos
./compiler -O arquivo.c         # liga as otimizações (combina com qualquer flag)
./compiler --help               # lista todas as flags
```

As flags podem ser combinadas, por exemplo `./compiler --tree --symtab
arquivo.c`. A saída sai sempre na ordem tokens, árvore, árvore com tipos,
DOT, tabela, TAC e bytecode. Com qualquer flag de análise ligada (`--tree`,
`--types`, `--dot`, `--symtab`, `--tac`, `--bytecode`) o Rust **não** é
impresso, porque a saída passa a ser a análise. O `--tokens` combinado com
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
  parser.y       # gramática; as ações montam a árvore e a tabela e checam os tipos
  ast.h/.c       # Node da árvore, construção/liberação, tree_print e tree_to_dot
  symtab.h/.c    # tabela de símbolos: hash por nome, pilha e histórico de escopos
  symtab_dump.h/.c # formatação da tabela pro --symtab
  semantic.h/.c  # semantic_error(), a checagem das chamadas e as regras de tipo
  optimize.h/.c  # passes com -O sobre a árvore: fold_constants, simplify_algebraic,
                 #   eliminate_dead_code e drop_after_jump
  codegen.h/.c   # gera o Rust a partir da árvore anotada (sai pelo stdout)
  tac.h/.c       # gerador de código de três endereços (--tac)
  tac_opt.h/.c   # otimização do TAC com -O (propagação e temporários mortos)
  bytecode.h/.c  # gerador de bytecode de máquina de pilha (--bytecode)
  types.h        # DataType, ValueData, type_to_rust() e type_to_c()
  options.h      # flags de CLI (OPT_*) e o switch emit_code
  main.c         # CLI: lê as flags, roda o parser e imprime o que foi pedido
tests/inputs/              # arquivos .c de entrada
tests/expected/            # saída esperada de cada input (.rs, .tree, .dot, .symtab, .err, .tac, .bc)
tests/run_tests.sh         # runner da suíte de regressão (chamado pelo make test)
tests/check_tree_tokens.sh # sanidade: folhas da árvore x tokens do léxico
tests/check_rust.sh        # ponta a ponta: rustc compila o Rust, e o exit code bate com o do gcc
tests/check_asan.sh        # roda todos os inputs sob ASan/UBSan, com e sem -O
```

No `parser.y`, o valor semântico de todo não-terminal é um `Node *`. A ação
de cada regra cria o nó com o helper `mk(label, linha, filhos..., NULL)`,
anota o `type` quando é expressão e deixa o Rust pro `codegen.c`. A linha de
cada símbolo vem de `@N.first_line` (`%locations`). `yylineno` não serve,
porque pode estar adiantado pelo lookahead.

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
| variável lida no próprio inicializador (`int x = x;`) | `variável 'x' usada na própria inicialização` |
| função não `void` que pode chegar ao fim sem `return` | `função 'f' pode chegar ao fim sem return` |

Todas saem em stderr no formato do erro sintático: `Erro semântico na linha
N: <mensagem>`. Os erros não param o parse, e todos saem no fim, ordenados
por linha. Programa com erro semântico não gera Rust e sai com exit code 1,
mas o `--tree` e o `--symtab` ainda imprimem a árvore e a tabela, o que
ajuda a depurar.

Como em C, a variável existe a partir do nome: em `int x = x;` o `x` da
direita já é a variável nova, mesmo com outro `x` visível de fora, e ela
ainda não tem valor. A checagem do `return` segue a regra do Rust: vale um
`return` no próprio bloco, um bloco `{ }` que sempre retorna ou um `if/else`
com `return` nos dois lados. Laço não conta, e o `main` fica de fora (em C
ele devolve 0 se chegar ao fim).

Como a gramática não tem protótipos, as chamadas de função são checadas só
no fim do programa. Assim, chamar uma função definida mais abaixo no arquivo
não é erro (o Rust também aceita funções em qualquer ordem).

### Tipos

Cada `expr` da árvore guarda o tipo em `Node->type` (`--types` imprime a
árvore anotada, ex. `expr [float]`). As regras são as de C:

- literal inteiro é `int`, literal com ponto é `double`; variável e parâmetro
  têm o tipo declarado; chamada tem o tipo de retorno da função;
- aritmética e comparação usam as conversões usuais (`char` < `int` < `float`
  < `double`): `int + float` é `float`, sem erro. Comparação e `&&`, `||`, `!`
  dão `int`;
- toda conversão numérica implícita (operando, atribuição, inicialização,
  argumento, `return`) vira um cast no Rust: `int x = f + 1;` fica
  `let mut x: i32 = ((f + (1 as f32)) as i32);`.

| erro | mensagem |
| ---- | -------- |
| `%`, `<<`, `>>`, `&`, `\|`, `^` com `float`/`double` | `operador '%' exige operandos inteiros (recebeu float e int)` |
| operando `void` (chamada de função `void` como valor) | `operador '+' não se aplica a operando void` |
| variável ou parâmetro `void` | `variável 'v' não pode ser do tipo void` |
| valor `void` atribuído, inicializado, passado ou retornado | `inicialização de 'a' com valor void` |
| `return expr` em função `void` | `função 'f' é void e não pode retornar valor` |
| condição `void` | `a condição do 'if' é void` |

Uma chamada a função definida **depois** do uso não tem o tipo conhecido
enquanto o parser lê a expressão. No fim do programa a árvore é percorrida de
novo e esses tipos são resolvidos (por isso o `--types` e o TAC saem certos),
mas os erros de tipo *dentro* dessas expressões não são checados: só os
argumentos `void` e a aridade. O Rust gerado usa a assinatura completa, então
os casts saem certos.

### Rust que compila

O codegen (`src/codegen.c`) cuida das diferenças que fariam o `rustc`
recusar o código:

- comparação, `&&`, `||` e `!` dão `bool` no Rust. Usados como número, viram
  `((a < b) as i32)`; numa condição, ficam como estão;
- número como condição (`if (x)`, `while (n)`, lado de `&&`/`||`) vira
  `(x != 0)` (`0.0` para float/double). O `!x` de um número vira `(x == 0)`,
  porque o `!` do Rust num inteiro inverte os bits;
- o `main` do Rust não devolve valor: `int main()` vira `fn main()`, e o
  `return v;` dele vira `std::process::exit(v);`. Programa sem `main` ganha um
  `fn main() {}` vazio.

## Código de três endereços (TAC)

`./compiler --tac` percorre a árvore já anotada e imprime o TAC, no lugar do
Rust. Cada função vira `func nome(params):` ... `endfunc`; cada operação
gasta um temporário (`t0`, `t1`, ...) e os saltos usam rótulos (`L0`, ...),
que recomeçam a cada função. Programa com erro não gera TAC.

`int x = a + b * c;` fica:

```
t0 = b * c
t1 = a + t0
x = t1
```

- `&&` e `||` têm curto-circuito, com `if t goto L` / `ifFalse t goto L`, e
  dão sempre 0 ou 1.
- As conversões implícitas aparecem (`t1 = (float) a`), a partir dos tipos da
  árvore. Isso vale pra operandos, atribuição, inicialização, argumento e
  `return`.
- Chamada: `param t` pra cada argumento, em ordem, e `t = call f, n`.
- `if`/`else`, `while` e `for` viram `ifFalse` + `goto`; `break` e `continue`
  saltam pro fim e pro próximo passo do laço (no `for`, a atualização).
- Variáveis de mesmo nome em escopos aninhados ganham um sufixo (`x` e
  `x.1`), porque o TAC não tem escopos.

### TAC otimizado (`--tac -O`)

Com `-O`, o TAC passa por `src/tac_opt.c` antes de sair. Cada função é
otimizada sozinha:

1. rótulo que nenhum salto usa sai, o que junta blocos básicos;
2. dentro de cada bloco básico (trecho sem rótulo, sem salto e sem `call`),
   propagação de constantes e de cópias. Só a aritmética inteira é dobrada,
   com as regras do constant folding da árvore (nada de divisão por zero,
   overflow ou shift fora de 0..31);
3. temporário que ninguém lê é removido (`call` nunca sai);
4. temporários e rótulos são renumerados em ordem.

`int a = 3; int b = a + 4; return b;` fica:

```
a = 3
b = 7
return 7
```

A propagação não passa de um bloco básico para outro. Sem `-O`, o TAC
continua o mesmo.

## Bytecode de máquina de pilha (`--bytecode`)

`./compiler --bytecode` percorre a mesma árvore anotada do TAC e imprime o
bytecode de uma máquina de pilha, no formato do exemplo do professor
(semana 10). Combina com `-O`: o bytecode sai da árvore já otimizada, então
`int x = 3 + 4;` vira só `LOAD_CONST 7` / `STORE x`.

`int x = a + b * c;` fica:

```
LOAD a
LOAD b
LOAD c
MUL
ADD
STORE x
```

Instruções: `LOAD_CONST`, `LOAD`, `STORE`, `ADD`, `SUB`, `MUL`, `DIV`,
`MOD`, `BIT_AND`, `BIT_OR`, `BIT_XOR`, `SHL`, `SHR`, `EQ`, `NE`, `LT`, `GT`,
`LE`, `GE`, `NOT`, `NEG`, `JUMP`, `JUMP_IF_FALSE`, `LABEL`, `CALL f n`,
`RET`. Conversões de tipo geram `I2F`, `I2D`, `F2I`, `F2D`, `D2I`, `D2F` a
partir do tipo anotado de cada nó (`int` e `char` são o mesmo tipo na
pilha).

- `&&` e `||` com curto-circuito (o segundo operando só é avaliado quando
  precisa);
- cada função começa com `LABEL nome`, `break`/`continue` viram `JUMP` pro
  fim e pro próximo passo do laço;
- chamada: o chamador empilha os argumentos em ordem; a função os tira com
  `STORE`, do último pro primeiro; `RET` devolve o valor do topo;
- escopos aninhados usam o mesmo esquema de alias do TAC (`x`, `x.1`),
  porque o bytecode também não tem escopos;
- `POP` está no conjunto mas nenhum código gera, porque o subconjunto não
  tem expressão como comando (só atribuição, declaração, `return`, `if` e
  laços).

Programa com erro léxico, sintático ou semântico não gera bytecode.

## Otimização (`-O`)

A flag `-O` liga todas as otimizações e combina com qualquer outra: sem flag
de análise sai o Rust otimizado, e com `--tree`, `--types`, `--tac` ou
`--bytecode` sai a análise do programa otimizado. Sem `-O`, nenhum passe
roda e a saída é idêntica à de antes.

Os passes rodam sempre na mesma ordem (ver `src/optimize.h`):

### 1. Constant folding — `src/optimize.c`, `fold_constants()`

Em cada `expr` que o parser reduz (pós-ordem), se os operandos forem
literais, troca a subárvore por um literal com o resultado e o tipo certos.
Respeita a divisão truncada de C (`7/2 → 3`, `5/2.0 → 2.5`) e cobre todos
os operadores (aritmética, `%`, bit a bit, shifts, comparações e `&&`, `||`,
`!`; comparações viram `true`/`false` no Rust e `0`/`1` no TAC).

| antes          | depois |
| -------------- | ------ |
| `2 + 3 * 4`    | `14`   |
| `(1+2)*(3+4)`  | `21`   |
| `5 == 2 + 3`   | `true` |

Não dobra o que mudaria o comportamento: divisão ou `%` por zero, overflow
de `int`, shift negativo ou ≥ 32, resultado float infinito/NaN, e expressão
com erro de tipo (ver `tests/inputs/opt_sem_folding.c`).

### 2. Simplificações algébricas — `simplify_algebraic()`

Na mesma `expr`, logo depois do folding, pra pegar os casos com variável
(que o folding não dobra):

| categoria          | antes                                              | depois            |
| ------------------ | -------------------------------------------------- | ----------------- |
| elemento neutro    | `x+0`, `0+x`, `x-0`, `x*1`, `1*x`, `x/1`, `x\|0`, `x^0`, `x<<0`, `x>>0` | `x` |
| mudança de tipo    | `x * 1.0` (com `x` int)                            | `x as f64`        |
| elemento absorvente | `x*0`, `x&0` (só int, sem chamada em `x`)         | `0`               |
| redução de força   | `x*2`, `2*x` (só int)                              | `x << 1`          |

Por rodar depois do folding no mesmo percurso, pega também `x * (3 - 2)` →
`x * 1` → `x`. `f() * 0` continua chamando `f()`, porque o absorvente é
bloqueado por efeito colateral. Não mexe em `x-x` nem em `x/x` (quebram com
float NaN e com `x = 0`).

### 3. Código morto — `eliminate_dead_code()` e `drop_after_jump()`

Chamado em cada `statement` que reduz, depois que as expressões dele já
passaram pelos passes 1 e 2:

| antes                                           | depois                      |
| ----------------------------------------------- | --------------------------- |
| `if (2 > 3) a = 1; else a = 2;`                 | `a = 2;`                    |
| `while (0) a = 1;`                              | *(apagado)*                 |
| `for (int i = 0; 0; i++) a = 1;`                | `int i = 0;` *(só o init)*  |
| `return 1; a = 2;`                              | `return 1;`                 |

O comando que vem depois de `return`, `break` ou `continue` na mesma lista é
apagado por `drop_after_jump`. Não gera aviso no stderr (iria quebrar os
testes `.err`).

### 4. Otimização do TAC — `src/tac_opt.c`

Só com `--tac -O`. Já descrito na seção **TAC otimizado** acima: remove
rótulos não usados, propaga constantes e cópias dentro de cada bloco básico,
e tira os temporários mortos. Roda por função, sobre o TAC já gerado da
árvore otimizada.

---

Os três exemplos da semana 9 (`tests/inputs/opt_semana9.c`) saem como `14`,
`5 + x * 4` e `21` no Rust e no TAC com `-O`.

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
- Protótipos de função (o `check_rust.sh` injeta na mão antes do gcc).
- Variável lida antes de receber valor (`int x; return x;`): é válido em C,
  mas o `rustc` recusa (E0381), então esses inputs ficam fora da parte 2
  dos testes de ponta a ponta.

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
- `erro_*.c`: um erro semântico por arquivo;
- `rust_*.c`: os casos em que o Rust gerado precisa de ajuste pra compilar
  (bool como valor, número como condição, `main`, programa sem `main` e
  chamada adiantada).
- `opt_*.c`: os casos do `-O` (folding da semana 9, o que não pode dobrar,
  código morto e propagação no TAC).

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
| `<nome>.types`     | `--types`    | stdout: a árvore com os tipos            |
| `<nome>.tac`       | `--tac`      | stdout: o código de três endereços       |
| `<nome>.bc`        | `--bytecode` | stdout: o bytecode de máquina de pilha   |
| `<nome>.err`       | *(nenhuma)*  | stderr, e o exit code tem que ser ≠ 0    |
| `<nome>.opt.<ext>` | `-O` + a de `<ext>` | o mesmo que `<ext>`, com otimização |

Com `.opt` antes da extensão o caso roda com `-O`: `opt_semana9.opt.rs` é o
Rust otimizado de `opt_semana9.c`, e `opt_semana9.opt.tac` é o
`-O --tac`. O mesmo input pode ter os dois esperados, com e sem `-O`.

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

### Ponta a ponta com `rustc` e `gcc`

Depois do `check_tree_tokens.sh`, o `make test` chama o
`tests/check_rust.sh`:

1. cada `.rs` de `tests/expected/` (inclusive os `.opt.rs` com `-O`) compila
   no `rustc` sem erro;
2. cada input válido com `main` roda no `gcc` e no `rustc`, sem e com `-O`,
   e o exit code tem que bater. Laço infinito estoura 5 s e conta como
   falha.

Sem `rustc`, o script avisa e passa; sem `gcc`, pula a parte 2. O
subconjunto deixa chamar função antes de definir e o gcc recusa declaração
implícita, então o script insere os protótipos no começo do `.c` antes de
compilar com gcc.

Ficam de fora da parte 2 (lista `SKIP` no script, com o motivo):
`tarefa2_controle_fluxo` e `tac_expressoes` leem variável sem valor (UB em
C, E0381 no rustc); `tac_otimizacao` e `opt_sem_folding` dividem por zero
de propósito.

### ASan/UBSan

O `make asan` builda o compilador de novo com `-fsanitize=address,undefined`
(em `compiler-asan`) e roda `tests/check_asan.sh`: todos os inputs, com e sem
`-O` e com as flags de análise, e falha se o ASan, o LeakSanitizer ou o UBSan
reclamarem. Serve pra pegar vazamento e *use-after-free* quando um passe do
`-O` troca uma subárvore. Fica fora do `make test` porque o build é mais
lento.

Pra verificar que a gramática não ganhou conflitos ao mexer no `parser.y`,
rode o bison direto com `-Wall`:

```bash
bison -d -Wall -o /tmp/parser.tab.c src/parser.y
```

Qualquer `shift/reduce` ou `reduce/reduce` aparece aí. Os únicos avisos
esperados hoje são os de `IF`/`ELSE`/... redeclarados.

## Acompanhamento

O trabalho está dividido nas issues do repositório, uma por entrega, com
assignee por pessoa:

- [Sprint 4: árvore de derivação (#12)](https://github.com/Edupizzol/Compiladores/issues/12),
  com as sub-issues #14 a #20;
- [Sprint 5: tabela de símbolos e fechamento (#13)](https://github.com/Edupizzol/Compiladores/issues/13),
  com as sub-issues #21 a #28;
- Sprint 6: verificação de tipos (#44);
- Sprint 7: código de três endereços (#45);
- [Sprint 8: otimização de código (#59)](https://github.com/Edupizzol/Compiladores/issues/59),
  com as sub-issues #49 a #53;
- [Sprint 9: geração de código final (#60)](https://github.com/Edupizzol/Compiladores/issues/60),
  com as sub-issues #54 a #58.

As tarefas são divididas por área da gramática pra minimizar conflito de
merge no `parser.y`.

### Co-autoria dos commits

Vários commits do repositório têm linhas `Co-Authored-By:` no fim da
mensagem. Elas marcam os dias em que a gente se encontrou presencialmente
(geralmente por conta das aulas) e revisou ou escreveu o código junto —
**não** querem dizer que a pessoa co-autora fez metade da tarefa, só que
estava ali e contribuiu.

Pra rastrear o que cada um fez (ou devia ter feito), o canal certo é a
aba de **issues**: cada entrega tem uma issue com assignee, e o PR que
fecha a issue aparece linkado nela. O histórico dos assignees e dos PRs
mergeados é a fonte da divisão real do trabalho.
