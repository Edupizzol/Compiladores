// tipos (issue #44): conversoes aritmeticas e anotacao da arvore
float media(int a, float b) {
    return (a + b) / 2;
}

int main() {
    int i = 3;
    float f = 2.5;
    double d = 1.0;
    char c;
    int r1 = i + f;
    double r2 = i * d + f;
    float r3 = media(i, i);
    int r4 = f > i;
    int r5 = i % 2 + (i << 1);
    f = i;
    i = f * 2;
    return -i;
}
