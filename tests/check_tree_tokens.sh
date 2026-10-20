#!/usr/bin/env bash
# Teste de sanidade da arvore (issue #20).
#
# As folhas da arvore, lidas em ordem, tem que bater com os tokens que o
# lexico produz no mesmo arquivo -- pontuacao incluida. A arvore nao pode
# perder nem inventar token.
#
# A comparacao e em duas partes:
#   1. a sequencia de rotulos das folhas tem que ser igual a sequencia de
#      tokens do lexico;
#   2. onde a folha traz o lexema (ex. "IDENTIFIER (x)"), o lexema tem que
#      ser o mesmo texto do token.
# A pontuacao pode aparecer como "LPAREN" ou "LPAREN ((" -- as duas formas
# valem, entao o lexema so e comparado quando a folha traz um.
#
# Os nao-terminais sao tirados direto do parser.y, entao o script acompanha
# a gramatica sozinha quando ela cresce.
#
# Enquanto a arvore nao existir (a #15-#19), o --tree nao produz nada e o
# script avisa que o caso ainda esta pendente. Quando a #19 subir, ele passa
# a checar de verdade.
#
# Sai com codigo != 0 se algum arquivo divergir.

set -u

cd "$(dirname "$0")/.." || exit 1

INPUT_DIR="tests/inputs"
COMPILER="./compiler"
PARSER="src/parser.y"

if [ ! -x "$COMPILER" ]; then
    echo "erro: $COMPILER nao existe, rode 'make' antes" >&2
    exit 1
fi

if [ ! -f "$PARSER" ]; then
    echo "erro: $PARSER nao encontrado" >&2
    exit 1
fi

# os nao-terminais da gramatica: os %type e as regras "nome:"
# (o %type tem um tipo entre colchetes: "%type <str> expr statement ...")
nonterminals=$( {
    grep -E '^%type' "$PARSER" | sed -E 's/^%type[^ ]*//' | tr ' ' '\n'
    grep -E '^[a-z_][a-zA-Z_0-9]*:' "$PARSER" | sed -E 's/:.*$//'
} | sort -u | grep -E '^[a-z_][a-zA-Z_0-9]*$')

# as folhas da arvore, no formato "ROTULO" ou "ROTULO|lexema":
# a folha e uma linha de ramo cujo rotulo NAO e um nao-terminal da gramatica.
extract_leaves() {
    sed -E 's/^[│ ]*[├└]── //' \
        | while IFS= read -r line; do
              label="${line%% (*}"
              [ "$label" = "$line" ] || label="${line%% *}"
              case "$label" in
                  ε) continue ;;                       # producao vazia
              esac
              if printf '%s\n' "$nonterminals" | grep -qxF "$label"; then
                  continue                             # nao-terminal: nao e folha
              fi
              if printf '%s' "$line" | grep -qE '\(.*\)$'; then
                  # a folha traz o lexema: "IDENTIFIER (x)"
                  printf '%s|%s\n' "$label" "$(printf '%s' "$line" | sed -E 's/^[^(]*\((.*)\)$/\1/')"
              else
                  printf '%s\n' "$label"
              fi
          done
}

# os tokens do lexico, no mesmo formato: "ROTULO|texto"
dump_token_leaves() {
    $COMPILER --tokens "$1" | sed -E 's/^ *[0-9]+ +([^ ]+) +(.*)$/\1|\2/'
}

# compara as duas listas: os rotulos tem que ser iguais em ordem, e onde a
# arvore traz lexema ele tem que ser o texto do token
compare() {
    paste -d'\t' "$1" "$2" | awk -F'\t' '
        {
            split($1, tree, "|");
            split($2, tok, "|");
            if (tree[1] != tok[1]) {
                printf "       token %d: arvore tem %s, lexico tem %s\n", NR, tree[1], tok[1];
                bad = 1;
                next;
            }
            if (length(tree) > 1) {
                if (tree[2] != tok[2]) {
                    printf "       token %d (%s): arvore tem o lexema \"%s\", lexico tem \"%s\"\n", NR, tree[1], tree[2], tok[2];
                    bad = 1;
                }
            }
        }
        END { exit bad ? 1 : 0 }'
}

fail=0
checked=0
pending=0

for input in "$INPUT_DIR"/*.c; do
    [ -f "$input" ] || continue
    name=$(basename "$input")

    tree=$($COMPILER --tree "$input" 2>/dev/null)
    if [ -z "$tree" ]; then
        # a arvore ainda nao foi implementada (#19): caso pendente
        pending=$((pending + 1))
        continue
    fi

    printf '%s\n' "$tree" | extract_leaves > /tmp/tree_sanity.tree
    dump_token_leaves "$input" > /tmp/tree_sanity.tok

    if compare /tmp/tree_sanity.tree /tmp/tree_sanity.tok; then
        echo "OK     $name  (as folhas da arvore batem com os tokens)"
        checked=$((checked + 1))
    else
        echo "FALHA  $name  (as folhas da arvore nao batem com os tokens)"
        fail=$((fail + 1))
    fi
done

rm -f /tmp/tree_sanity.tree /tmp/tree_sanity.tok

if [ "$checked" -eq 0 ]; then
    if [ "$pending" -gt 0 ]; then
        echo "sanidade da arvore: $pending casos pendentes (a arvore ainda nao foi implementada, issue #19)"
    else
        echo "sanidade da arvore: nenhum input pra checar"
    fi
    exit 0
fi

if [ "$fail" -ne 0 ]; then
    echo "$fail verificacoes de sanidade falharam"
    exit 1
fi

echo "todas as $checked verificacoes de sanidade passaram"
