int fatorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * fatorial(n - 1);
}

double media(int a, double b) {
    return (a + b) / 2;
}

int sub(int a, int b) {
    return a - b;
}

int main() {
    int r = sub(10, 3);
    double m = media(3, 4.0);
    int k = m;
    return fatorial(5) + r + k;
}
