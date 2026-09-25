// cobre os tokens da issue #2: rode com ./compiler --tokens tests/inputs/lexico.c
/* comentario de bloco
   em varias linhas: a contagem de linha tem que continuar certa */
struct ponto {
    double x;
    double y;
};

void imprime(char c, char *s) {
    char fim = '\0';
    char quebra = '\n';
    s = "ola, \"mundo\"\n";
}

int main() {
    double d = 3.14;
    double e = .5e-3;
    float f = 2.0;
    int i = 0;
    struct ponto p;
    struct ponto *pp;
    p.x = d;
    pp->y = e;

    if (i < 10 && i > 0 || i != 5) {
        i++;
    } else if (i <= 3 || i >= 7 || i == 4) {
        i--;
    }

    while (!(i >= 100)) {
        i += 2;
        i -= 1;
        i *= 3;
        i /= 2;
        i %= 7;
        i &= 15; i |= 1; i ^= 2;
        i <<= 1; i >>= 1;
        if (i == 50) break;
    }

    for (i = 0; i < 10; i++) {
        if (i % 2 == 0) continue;
    }
    return i / 2;
}
