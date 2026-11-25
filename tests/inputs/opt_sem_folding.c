// O que o -O NAO pode dobrar (issue #53): mudaria o comportamento do
// programa. Divisao e % por zero, overflow de int, shift fora de 0..31 e
// f() * 0, que continua chamando f()
int f() {
    return 7;
}

int main() {
    int a = 5 / 0;
    int b = 5 % (2 - 2);
    int c = 2147483647 + 1;
    int d = 1 << 32;
    int e = f() * 0;
    int g = a * 0;
    return e + g;
}
