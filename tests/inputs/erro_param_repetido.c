// erro_param_repetido.c (issue #26): parametro repetido na mesma funcao
int f(int a, int b, int a) {
    return a + b;
}
