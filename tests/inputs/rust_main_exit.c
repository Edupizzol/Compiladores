// Rust que compila (issue #55): o main do Rust nao devolve valor, entao o
// return do main de C vira std::process::exit
int dobro(int v) {
    return v * 2;
}

int main() {
    int r = dobro(21);
    if (r > 40) {
        return r;
    }
    return 0;
}
