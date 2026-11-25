// Codigo morto com -O (issue #53): if com condicao constante depois do
// folding, while e for que nunca rodam, e comando depois de return/break
int main() {
    int x = 0;
    if (2 > 3) {
        x = 1;
    } else {
        x = 2;
    }
    if (0)
        x = 10;
    if (1 + 1 == 2) {
        int y = 5;
        x = x + y;
    }
    while (1 > 2) {
        x = x + 100;
    }
    for (int i = 0; 0; i = i + 1) {
        x = x + 1000;
    }
    while (x < 100) {
        x = x + 10;
        break;
        x = 0;
    }
    return x;
    x = 99;
}
