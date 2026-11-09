// TAC (issue #45): if/else, while, for, break/continue, && e || com curto-circuito
int main() {
    int i = 0;
    int soma = 0;
    while (i < 10) {
        i = i + 1;
        if (i == 3) {
            continue;
        }
        if (i > 8) {
            break;
        }
        soma = soma + i;
    }
    for (int j = 0; j < 5; j = j + 1) {
        if (j % 2 == 0 && soma > 0 || j == 4) {
            soma = soma - 1;
        } else {
            soma = soma + 2;
        }
    }
    for (;;) {
        break;
    }
    return soma;
}
