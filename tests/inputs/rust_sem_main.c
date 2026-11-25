// Rust que compila (issue #55): programa sem main ganha um `fn main() {}`
// vazio, senao o rustc nao gera o executavel
int quadrado(int v) {
    return v * v;
}
