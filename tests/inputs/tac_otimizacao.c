// TAC otimizado (issue #52): propagacao de constantes e copias, temporarios
// mortos e renumeracao. Sem -O o TAC tem que sair igual ao de sempre
int quadrado(int n) {
    int m = n;
    return m * m;
}

int main() {
    int a = 3;
    int b = a + 4;
    int c = b * 2 - a;
    int d = c / 0;
    int e = quadrado(b) + c;
    for (int i = 0; i < 3; i = i + 1) {
        e = e + i;
    }
    return b;
}
