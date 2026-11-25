// Propagacao de constantes e copias no TAC com -O (issue #53): a variavel
// que recebe uma constante passa o valor adiante dentro do bloco basico
int main() {
    int a = 3;
    int b = a + 4;
    int c = b;
    int d = c * 2;
    while (d > 0) {
        d = d - b;
    }
    return d + c;
}
