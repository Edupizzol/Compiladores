// erro de tipo (issue #44): return com valor em funcao void e com valor void
void f() {
    return 1;
}

void g() {
    int x;
}

int h() {
    return g();
}
