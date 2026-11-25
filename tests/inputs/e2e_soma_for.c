// Ponta a ponta (issue #57): soma com for, break e continue. O continue de
// um for tem que rodar o passo antes de voltar pra condicao
int main() {
    int soma = 0;
    for (int i = 1; i <= 100; i = i + 1) {
        if (i % 2 == 0)
            continue;
        if (soma > 50)
            break;
        soma = soma + i;
    }
    int pares = 0;
    for (int j = 0; j < 10; j = j + 1) {
        int k = 0;
        while (k < 3) {
            k = k + 1;
            if (k == 2)
                continue;
            pares = pares + 1;
        }
        if (j == 1 * 4)
            continue;
        pares = pares + 10;
    }
    return soma + pares;
}
