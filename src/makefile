# Makefile - transpiler com Flex + Bison
# Ajuste os nomes de arquivo (src/parser.y, src/lexer.l, etc.) conforme o projeto de vocês.

CC      = gcc
CFLAGS  = -Wall -g
TARGET  = compiler

SRC_DIR = src

# Arquivos C "normais" do projeto (ast, codegen, symtab, main, etc.)
# Adicione/remova conforme forem criando os módulos
EXTRA_SRCS = $(SRC_DIR)/ast.c $(SRC_DIR)/codegen.c $(SRC_DIR)/symtab.c $(SRC_DIR)/main.c

# Arquivos gerados por bison/flex
BISON_OUT = parser.tab.c
BISON_HDR = parser.tab.h
FLEX_OUT  = lex.yy.c

.PHONY: all clean run

all: $(TARGET)

# Bison gera o parser.tab.c e o parser.tab.h (com -d)
$(BISON_OUT) $(BISON_HDR): $(SRC_DIR)/parser.y
	bison -d -o $(BISON_OUT) $(SRC_DIR)/parser.y

# Flex depende do header do bison (pra conhecer os tokens)
$(FLEX_OUT): $(SRC_DIR)/lexer.l $(BISON_HDR)
	flex -o $(FLEX_OUT) $(SRC_DIR)/lexer.l

# Linkagem final
$(TARGET): $(BISON_OUT) $(FLEX_OUT) $(EXTRA_SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(BISON_OUT) $(FLEX_OUT) $(EXTRA_SRCS)

# Roda o compilador em cima de um arquivo de teste
run: $(TARGET)
	./$(TARGET) tests/inputs/exemplo.rs

clean:
	rm -f $(TARGET) $(BISON_OUT) $(BISON_HDR) $(FLEX_OUT)
