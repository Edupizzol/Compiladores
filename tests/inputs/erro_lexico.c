// erro_lexico.c (issue #28): caractere desconhecido invalida o programa,
// mesmo que o parser consiga seguir (sem Rust e exit != 0)
int main() {
    int a = 1;
    return a; @
}
