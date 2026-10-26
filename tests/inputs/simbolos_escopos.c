// simbolos_escopos.c (issue #26)
// sombreamento, blocos aninhados e a variavel do for, que nao vaza
int main() {
    int x = 1;
    int y = 2;
    {
        int x = 3;
        {
            double y = 4.0;
            x = x + 1;
        }
    }
    for (int i = 0; i < 3; i = i + 1) {
        int t = i;
        x = x + t;
    }
    for (int i = 10; i > 0; i = i - 1) {
        y = y + i;
    }
    { }
    return x + y;
}
