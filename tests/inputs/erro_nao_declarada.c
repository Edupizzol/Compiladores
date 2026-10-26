// erro_nao_declarada.c (issue #26): uso e atribuicao sem declarar
int main() {
    int a = 1;
    a = b + 1;
    c = a;
    for (j = 0; a < 2; a = a + 1) {
        a = a;
    }
    return a;
}
