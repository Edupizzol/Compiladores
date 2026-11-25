// Ponta a ponta (issue #57): fatorial recursivo e iterativo. O resultado sai
// pelo exit code do main, que tem que ser o mesmo no gcc e no rustc
int fat(int n) {
    if (n <= 1)
        return 1;
    return n * fat(n - 1);
}

int fat_iter(int n) {
    int r = 1;
    while (n > 1) {
        r = r * n;
        n = n - 1;
    }
    return r;
}

int main() {
    int a = fat(5);
    int b = fat_iter(2 + 2);
    return a - b * (3 - 2);
}
