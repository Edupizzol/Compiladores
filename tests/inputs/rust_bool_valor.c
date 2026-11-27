// Rust que compila (issue #55): comparacao, && || e ! usados como numero
// saem com `as i32`; o ! de um int e `== 0` (no Rust, ! num i32 inverte os bits)
int main() {
    int a = 3;
    int b = a < 5;
    int c = (a == 3) + (a != 3);
    int d = a > 1 && b;
    int e = !a;
    int f = !(a < b);
    double g = (a >= 3) * 2.5;
    int h = (a < b) == (b < a);
    return b + c + d + e + f + h;
}
