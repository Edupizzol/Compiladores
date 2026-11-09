// erro de tipo (issue #44): void nao e valor
void nada(int x) {
    return x;
}

int soma(int a, int b) {
    return a + b;
}

int main() {
    void v;
    int a = nada(1);
    int b = nada(1) + 2;
    int c = soma(nada(1), 2);
    int d = -nada(2);
    int e;
    e = nada(3);
    if (nada(4)) {
        e = 1;
    }
    return later(nada(5));
}

int later(int x) {
    return x;
}

void param_void(void p) {
    int x;
}
