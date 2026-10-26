// simbolos_funcoes.c (issue #26)
// varias funcoes, recursao e chamada de funcao definida depois
int fatorial(int n) {
    if (n <= 1)
        return 1;
    return n * fatorial(n - 1);
}

int main() {
    int r = fatorial(5);
    double m = media(1.0, 2.0);
    return r;
}

double media(double a, double b) {
    return (a + b) / 2.0;
}

void nada() {
    int vazio = 0;
}
