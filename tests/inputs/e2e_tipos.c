// Ponta a ponta (issue #57): conversoes entre int, double e char, e a
// divisao inteira truncando como em C
double media(int a, int b) {
    return (a + b) / 2.0;
}

int main() {
    char c = 97;
    int x = 7 / 2;
    double m = media(3, 4);
    int t = m * 10;
    int d = -7 / 2;
    int r = -7 % 3;
    return c + x + t + d + r;
}
