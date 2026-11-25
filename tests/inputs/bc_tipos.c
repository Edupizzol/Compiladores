int main() {
    int i = 7;
    float f = i;
    double d = f + 2.5;
    int back = d;
    char c = 65;
    int soma = c + 1;
    double misto = i * 1.5 + c;
    if (-back < 0) {
        soma = soma + 1;
    }
    return soma + back;
}
