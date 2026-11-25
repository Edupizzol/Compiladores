// Ponta a ponta (issue #57): fibonacci iterativo com for, e recursivo
int fib(int n) {
    if (n < 2)
        return n;
    return fib(n - 1) + fib(n - 2);
}

int main() {
    int a = 0;
    int b = 1;
    for (int i = 0; i < 12; i = i + 1) {
        int t = a + b;
        a = b;
        b = t;
    }
    if (a != fib(12))
        return 1;
    return a % 256;
}
