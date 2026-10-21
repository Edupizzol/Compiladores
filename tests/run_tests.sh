#!/usr/bin/env bash
# Suite de regressao do compilador.
#
# Cada arquivo em tests/expected/ e um caso de teste. O nome do arquivo
#expected/<nome>.<ext> corresponde ao input tests/inputs/<nome>.c, e a
# extensao decide a flag passada pro compilador:
#
#   .rs      -> sem flag, compara o stdout (o Rust gerado)
#   .tree    -> --tree,   compara o stdout (a arvore em texto)
#   .symtab  -> --symtab, compara o stdout (a tabela de simbolos)
#   .err     -> sem flag, espera exit code != 0 e compara o stderr
#
# Sai com codigo 0 se todos passarem, != 0 se algum falhar.

set -u

cd "$(dirname "$0")/.." || exit 1

INPUT_DIR="tests/inputs"
EXPECTED_DIR="tests/expected"
COMPILER="./compiler"

if [ ! -x "$COMPILER" ]; then
    echo "erro: $COMPILER nao existe, rode 'make' antes" >&2
    exit 1
fi

if [ ! -d "$EXPECTED_DIR" ]; then
    echo "erro: $EXPECTED_DIR nao existe" >&2
    exit 1
fi

pass=0
fail=0
failed_cases=()

# a flag de cada extensao de esperado (vazia = sem flag)
flag_for() {
    case "$1" in
        .tree)   echo "--tree" ;;
        .symtab) echo "--symtab" ;;
        .dot)    echo "--dot" ;;
        *)       echo "" ;;
    esac
}

for expected in "$EXPECTED_DIR"/*; do
    [ -f "$expected" ] || continue

    name=$(basename "$expected")
    base="${name%.*}"
    ext="${name##*.}"
    [ "$base" = "$name" ] && continue   # sem extensao: ignora

    input="$INPUT_DIR/$base.c"
    if [ ! -f "$input" ]; then
        echo "FALHA  $name  (input correspondente nao encontrado: $input)"
        fail=$((fail + 1))
        failed_cases+=("$name")
        continue
    fi

    flag=$(flag_for ".$ext")
    actual=$(mktemp)
    expected_file=$(mktemp)

    if [ "$ext" = "err" ]; then
        # espera erro: exit code != 0 e stderr igual ao esperado
        $COMPILER $flag "$input" >/dev/null 2>"$actual"
        status=$?
        if [ "$status" -eq 0 ]; then
            echo "FALHA  $name  (esperava exit code != 0, mas deu 0)"
            fail=$((fail + 1))
            failed_cases+=("$name")
            rm -f "$actual" "$expected_file"
            continue
        fi
        cp "$expected" "$expected_file"
    else
        # caso normal: stdout tem que bater com o esperado
        $COMPILER $flag "$input" >"$actual" 2>/dev/null
        status=$?
        if [ "$status" -ne 0 ]; then
            echo "FALHA  $name  (compilador falhou com exit code $status)"
            fail=$((fail + 1))
            failed_cases+=("$name")
            rm -f "$actual" "$expected_file"
            continue
        fi
        cp "$expected" "$expected_file"
    fi

    if diff -u "$expected_file" "$actual" > /dev/null; then
        echo "OK     $name"
        pass=$((pass + 1))
    else
        echo "FALHA  $name"
        diff -u "$expected_file" "$actual" | sed 's/^/       /'
        fail=$((fail + 1))
        failed_cases+=("$name")
    fi

    rm -f "$actual" "$expected_file"
done

echo
if [ "$fail" -eq 0 ]; then
    echo "todos os $pass testes passaram"
    exit 0
fi

echo "$pass passaram, $fail falharam:"
for c in "${failed_cases[@]}"; do
    echo "  - $c"
done
exit 1
