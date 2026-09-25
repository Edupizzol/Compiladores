// tarefa 1 (issue #4): expressoes e operadores
// rode com ./compiler tests/inputs/tarefa1_expressoes.c
int zero() {
    return 0;
}

int soma(int a, int b) {
    return a + b;
}

double media(double x, double y) {
    return (x + y) / 2.0;
}

int main() {
    int a = 7;
    int b = 3;

    // aritmetica e precedencia: * / % antes de + -, associatividade a esquerda
    int r1 = a + b * 2 - a / b % 2;
    int r2 = a - b - 1;
    int r3 = (a + b) * (a - b);

    // unarios
    int r4 = -a + -(b * 2);
    int r5 = !a;
    int r6 = - -a;

    // relacionais e igualdade: < > <= >= antes de == !=
    int r7 = a < b == b > a;
    int r8 = a <= b != a >= b;

    // logicos: && antes de ||
    int r9 = a > 0 || b > 0 && !(a == b);

    // bitwise: shift antes de relacional, relacional antes de & ^ |
    int r10 = a & b == 3;
    int r11 = a | b ^ a & b;
    int r12 = a << 2 >> 1 < b;

    // chamada de funcao como expressao: sem argumentos, varios, aninhada
    int c1 = zero();
    int c2 = soma(a, b * 2);
    int c3 = soma(soma(a, 1), zero()) + 1;
    double m = media(1.5, 2.5) * 2.0;

    return soma(r1, c3) > 10 && a != b;
}
