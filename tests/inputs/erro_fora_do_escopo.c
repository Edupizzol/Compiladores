// erro_fora_do_escopo.c (issue #26): variavel usada depois do escopo dela
int main() {
    {
        int x;
    }
    x = 1;
    for (int i = 0; i < 2; i = i + 1) {
    }
    return i;
}
