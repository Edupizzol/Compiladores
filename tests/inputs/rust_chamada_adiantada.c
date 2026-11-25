// Rust que compila (issue #55): chamada a funcao definida depois do uso
// tambem ganha os casts, pela assinatura que a tabela tem no fim do parse
int main() {
    int a = metade(7);
    double b = metade(3) + 1;
    return a + positivo(b);
}

double metade(int v) {
    return v / 2.0;
}

int positivo(double x) {
    return x > 0;
}
