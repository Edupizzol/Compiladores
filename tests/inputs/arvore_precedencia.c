// arvore_precedencia.c (issue #20)
// casos de aceite da issue #18: a arvore tem que mostrar a precedencia e a
// associatividade de C
int f(int a, int b) {
    return a;
}

int g(int a) {
    return a;
}

int main() {
    int a = 1;
    int b = 2;
    int c = 3;
    int p1 = a + b * c;
    int p2 = a - b - c;
    int p3 = -a * b;
    int p4 = !a && b;
    int p5 = f(a, g(b));
    return p1;
}
