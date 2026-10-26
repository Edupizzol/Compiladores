// erro_funcao_duplicada.c (issue #26): funcao definida duas vezes
int f(int a) {
    return a;
}

int g() {
    return 0;
}

float f(float b) {
    return b;
}
