// TAC (issue #45): expressoes, precedencia, conversoes e chamadas
int soma(int a, int b) {
    return a + b;
}

float media(float x, float y) {
    return (x + y) / 2;
}

int main() {
    int a = 1;
    int b = 2;
    int c = 3;
    int x;
    x = a + b * c;
    int y = -(a - b) * !c;
    float f = 1.5;
    float g = a + f;
    int h = f > a;
    int k = soma(a, b * 2) + soma(1, soma(b, c));
    float m = media(a, b);
    char ch;
    int z = ch + 1;
    return x << 2 | y & 7;
}
