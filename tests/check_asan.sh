#!/usr/bin/env bash
# Checagem de memoria com ASan/UBSan (issue #53).
#
# O -O troca subarvores inteiras (folding, algebricas, codigo morto), entao
# um no esquecido vaza e um no liberado cedo demais vira use-after-free. O
# valgrind nao esta instalado, entao o compilador e buildado de novo com
# -fsanitize=address,undefined (o `make asan` faz isso, em compiler-asan) e
# roda em todos os inputs, com e sem -O, em todas as combinacoes de flag.
#
# Os inputs com erro (.err) tambem entram: o caminho de erro tem que liberar
# a memoria do mesmo jeito. Conta como falha qualquer relatorio do ASan, do
# LeakSanitizer ou do UBSan no stderr.
#
# Sai com codigo != 0 se algum caso falhar.

set -u

cd "$(dirname "$0")/.." || exit 1

INPUT_DIR="tests/inputs"
COMPILER="./compiler-asan"

if [ ! -x "$COMPILER" ]; then
    echo "erro: $COMPILER nao existe, rode 'make asan'" >&2
    exit 1
fi

export ASAN_OPTIONS="detect_leaks=1"
export UBSAN_OPTIONS="print_stacktrace=1"

# cada linha e uma combinacao de flags (a vazia e a saida Rust)
combos=(
    ""
    "-O"
    "--tac"
    "-O --tac"
    "--bytecode"
    "-O --bytecode"
    "--tree --types --dot --symtab"
    "-O --tree --types --dot --symtab"
    "--tokens -O --tac"
)

fail=0
runs=0
log=$(mktemp)

for input in "$INPUT_DIR"/*.c; do
    [ -f "$input" ] || continue
    for flags in "${combos[@]}"; do
        $COMPILER $flags "$input" >/dev/null 2>"$log"
        runs=$((runs + 1))
        if grep -qE 'ERROR: (AddressSanitizer|LeakSanitizer)|runtime error:' "$log"; then
            echo "FALHA  $(basename "$input")  [${flags:-sem flag}]"
            grep -E 'ERROR:|runtime error:|SUMMARY:' "$log" | sed 's/^/       /'
            fail=$((fail + 1))
        fi
    done
done

rm -f "$log"

if [ "$fail" -ne 0 ]; then
    echo "$fail de $runs execucoes com erro de memoria"
    exit 1
fi
echo "ASan/UBSan: as $runs execucoes estao limpas"
