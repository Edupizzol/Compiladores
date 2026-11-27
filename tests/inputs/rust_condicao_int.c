// Rust que compila (issue #55): int e double como condicao viram `!= 0`,
// e o && || com int compara cada lado com zero
int main() {
    int n = 3;
    int soma = 0;
    double x = 0.5;
    while (n) {
        soma = soma + n;
        n = n - 1;
    }
    if (x) {
        soma = soma + 10;
    }
    for (int i = 4; i; i = i - 1) {
        if (i % 2 && soma) {
            soma = soma + 1;
        }
    }
    if (!x || n) {
        soma = 0;
    }
    return soma;
}
