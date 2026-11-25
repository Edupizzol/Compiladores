// Os tres exemplos de constant folding da semana 9 (issue #53), com -O:
// saem dobrados como 14, 5 + x * 4 e 21 no Rust e no TAC
int main() {
    int x = 3;
    int a = 2 + 3 * 4;
    int b = 5 + x * (2 + 2);
    int c = (1 + 2) * (3 + 4);
    return a + b + c;
}
