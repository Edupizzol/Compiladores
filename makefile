# Makefile - transpiler com Flex + Bison
# Ajuste os nomes de arquivo (src/parser.y, src/lexer.l, etc.) conforme o projeto de vocês.

CC      = gcc
CFLAGS = -Wall -g -Isrc
TARGET  = compiler

SRC_DIR = src
TEST_DIR = tests
INPUT_DIR = $(TEST_DIR)/inputs
EXPECTED_DIR = $(TEST_DIR)/expected

# Arquivos C "normais" do projeto (ast, codegen, symtab, main, etc.)
# O wildcard pega tudo que aparecer em src/, entao ast.c e symtab.c entram
# no build sem ninguem mexer no makefile (e sem conflito de merge).
EXTRA_SRCS = $(wildcard $(SRC_DIR)/*.c)

# Headers do projeto: mudanca em qualquer um deles refaz o binario
HEADERS = $(wildcard $(SRC_DIR)/*.h)

# Arquivos gerados por bison/flex
BISON_OUT = parser.tab.c
BISON_HDR = parser.tab.h
FLEX_OUT  = lex.yy.c

.PHONY: all clean run test

all: $(TARGET)

# Bison gera o parser.tab.c e o parser.tab.h (com -d)
$(BISON_OUT) $(BISON_HDR): $(SRC_DIR)/parser.y
	bison -d -o $(BISON_OUT) $(SRC_DIR)/parser.y

# Flex depende do header do bison (pra conhecer os tokens)
$(FLEX_OUT): $(SRC_DIR)/lexer.l $(BISON_HDR)
	flex -o $(FLEX_OUT) $(SRC_DIR)/lexer.l

# Linkagem final
$(TARGET): $(BISON_OUT) $(FLEX_OUT) $(EXTRA_SRCS) $(HEADERS)
	$(CC) $(CFLAGS) -o $(TARGET) $(BISON_OUT) $(FLEX_OUT) $(EXTRA_SRCS)

# Roda o compilador em cima de um arquivo de teste
run: $(TARGET)
	./$(TARGET) $(INPUT_DIR)/exemplo.c

# Roda a suite de regressao: pra cada arquivo em tests/expected/, compara a
# saida do compilador com a esperada. A flag depende da extensao do esperado:
#   .rs      -> sem flag (stdout: o Rust gerado)
#   .tree    -> --tree   (stdout: a arvore em texto)
#   .symtab  -> --symtab (stdout: a tabela de simbolos)
#   .err     -> espera exit code != 0 e compara o stderr
# Imprime OK/FALHA por arquivo, mostra o diff na falha e sai com codigo
# != 0 se algum falhar.
test: $(TARGET)
	@bash $(TEST_DIR)/run_tests.sh

clean:
	rm -f $(TARGET) $(BISON_OUT) $(BISON_HDR) $(FLEX_OUT)
