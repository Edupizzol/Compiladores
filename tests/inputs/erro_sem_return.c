// funcao com retorno que pode chegar ao fim sem return (issue #55): em Rust
// nao compila. Laco nao conta, como no rustc; if/else com return dos dois
// lados conta
int sinal(int x) {
    if (x > 0) {
        return 1;
    } else {
        return 0;
    }
}

int sem_else(int x) {
    if (x > 0) {
        return 1;
    }
}

int laco(int x) {
    while (x) {
        return x;
    }
}

int main() {
    return sinal(1) + sem_else(2) + laco(3);
}
