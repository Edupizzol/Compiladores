int f(int a) {
    if (a) {
        return 1;
    }
    return 2;
}

int main() {
    int x = 0;
    while (x < 3) {
        x = x + 1;
    }
    return f(x);
}
