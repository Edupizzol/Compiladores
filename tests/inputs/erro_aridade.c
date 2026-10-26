// erro_aridade.c (issue #26): numero de argumentos e funcao nao declarada
int f(int a) {
    return a;
}

int main() {
    int x = f(1, 2);
    int y = f();
    int z = g(x);
    return f(x) + h();
}
