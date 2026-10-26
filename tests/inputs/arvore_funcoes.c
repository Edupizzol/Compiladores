// arvore_funcoes.c (issue #20)
// varias funcoes, parametros e chamada aninhada
int soma(int a, int b) {
    return a + b;
}

int main() {
    int x = 1;
    int y = 2;
    int z = soma(soma(x, y), x);
    return z;
}
