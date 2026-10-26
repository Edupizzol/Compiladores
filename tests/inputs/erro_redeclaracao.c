// erro_redeclaracao.c (issue #26): redeclaracao no mesmo escopo
// (sombrear num bloco interno nao e erro)
int f(int p) {
    int x = 1;
    {
        int x = 2;
    }
    int x = 3;
    int p = 4;
    return x;
}
