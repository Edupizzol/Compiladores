#!/usr/bin/env bash
# Testes de ponta a ponta do Rust gerado (issue #57).
#
# Os .rs de tests/expected/ provam que o Rust nao mudou, mas nao que ele
# compila nem que faz o mesmo que o C. Este script fecha isso em duas partes:
#
#   1. cada .rs esperado (inclusive os .opt.rs) tem que compilar no rustc;
#   2. cada input valido com `main` e compilado com o gcc e, pelo nosso
#      compilador, com o rustc, sem e com -O. Os tres programas rodam e o
#      exit code tem que ser o mesmo: o subconjunto nao tem printf, entao o
#      resultado sai pelo `return` do main (que no Rust vira process::exit).
#
# O C nao tem prototipos no subconjunto (a funcao pode ser chamada antes de
# definida), e o gcc recusa a declaracao implicita. Entao os prototipos sao
# tirados das definicoes e colocados no comeco antes de passar pro gcc.
#
# Sem o rustc o script so avisa e sai com 0; sem o gcc, pula a parte 2.
# Sai com codigo != 0 se algum caso falhar.

set -u

cd "$(dirname "$0")/.." || exit 1

INPUT_DIR="tests/inputs"
EXPECTED_DIR="tests/expected"
COMPILER="./compiler"

# inputs que ficam de fora, com o motivo (o README lista os mesmos)
declare -A SKIP=(
    [tarefa2_controle_fluxo]="lê variável sem valor (if (i) com int i;): UB em C, e o rustc recusa (E0381)"
    [tac_expressoes]="lê variável sem valor (char ch;): UB em C, e o rustc recusa (E0381)"
    [tac_otimizacao]="divide por zero de propósito (c / 0): UB em C, e o rustc recusa (unconditional_panic)"
    [opt_sem_folding]="divide por zero de propósito (5 / 0): UB em C, e o rustc recusa (unconditional_panic)"
)

if [ ! -x "$COMPILER" ]; then
    echo "erro: $COMPILER nao existe, rode 'make' antes" >&2
    exit 1
fi

if ! command -v rustc >/dev/null 2>&1; then
    echo "aviso: rustc nao encontrado, testes de ponta a ponta pulados"
    exit 0
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

pass=0
fail=0
skipped=0

# compila um .rs; o --crate-name evita o erro com `.` no nome (x.opt.rs)
rustc_build() {
    rustc --edition 2021 -A warnings --crate-name e2e -o "$2" "$1" 2>"$work/rustc.log"
}

# roda um binario e devolve o exit code (124 se passar de 5 s: laco infinito)
run_exit() {
    timeout 5 "$1" >/dev/null 2>&1
    echo $?
}

# prototipos das funcoes de um .c: a linha da definicao sem o `{`
prototypes() {
    grep -E '^(int|float|double|char|void)[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\(' "$1" \
        | sed -E 's/[[:space:]]*\{.*$//; s/$/;/'
}

# ---- parte 1: os .rs esperados compilam ----

for rs in "$EXPECTED_DIR"/*.rs; do
    [ -f "$rs" ] || continue
    name=$(basename "$rs")
    base="${name%.rs}"
    base="${base%.opt}"
    if [ -n "${SKIP[$base]+x}" ]; then
        echo "PULADO $name  (${SKIP[$base]})"
        skipped=$((skipped + 1))
        continue
    fi
    if rustc_build "$rs" "$work/bin"; then
        echo "OK     $name  (compila no rustc)"
        pass=$((pass + 1))
    else
        echo "FALHA  $name  (nao compila no rustc)"
        grep -E '^error' "$work/rustc.log" | sed 's/^/       /'
        fail=$((fail + 1))
    fi
done

# ---- parte 2: o Rust faz o mesmo que o C, com e sem -O ----

if ! command -v gcc >/dev/null 2>&1; then
    echo "aviso: gcc nao encontrado, comparacao com o C pulada"
else
    for input in "$INPUT_DIR"/*.c; do
        [ -f "$input" ] || continue
        name=$(basename "$input")
        base="${name%.c}"
        [ -n "${SKIP[$base]+x}" ] && continue
        grep -qE '^int[[:space:]]+main[[:space:]]*\(' "$input" || continue
        # input invalido (os erro_*) nao gera Rust
        $COMPILER "$input" >"$work/plain.rs" 2>/dev/null || continue

        { prototypes "$input"; cat "$input"; } >"$work/prog.c"
        if ! gcc -w -o "$work/c.bin" "$work/prog.c" 2>"$work/gcc.log"; then
            echo "FALHA  $name  (nao compila no gcc)"
            sed 's/^/       /' "$work/gcc.log"
            fail=$((fail + 1))
            continue
        fi
        want=$(run_exit "$work/c.bin")

        for opt in "" "-O"; do
            label="${opt:-sem -O}"
            $COMPILER $opt "$input" >"$work/prog.rs" 2>/dev/null
            if ! rustc_build "$work/prog.rs" "$work/rs.bin"; then
                echo "FALHA  $name  [$label]  (o Rust gerado nao compila)"
                grep -E '^error' "$work/rustc.log" | sed 's/^/       /'
                fail=$((fail + 1))
                continue
            fi
            got=$(run_exit "$work/rs.bin")
            if [ "$got" = "$want" ]; then
                echo "OK     $name  [$label]  (gcc e rustc saem com $want)"
                pass=$((pass + 1))
            else
                echo "FALHA  $name  [$label]  (gcc saiu com $want, rustc com $got)"
                fail=$((fail + 1))
            fi
        done
    done
fi

echo
if [ "$fail" -ne 0 ]; then
    echo "ponta a ponta: $pass passaram, $fail falharam"
    exit 1
fi
echo "ponta a ponta: todos os $pass casos passaram ($skipped .rs pulados, ver README)"
