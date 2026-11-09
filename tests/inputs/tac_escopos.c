// TAC (issue #45): variaveis de mesmo nome em escopos aninhados, funcao definida depois do uso
int main() {
    int x = 1;
    {
        int x = 2;
        x = x + 1;
    }
    double d = 0.5;
    d = later(x) + d;
    return x;
}

int later(int p) {
    int x = p;
    return x * 2;
}
