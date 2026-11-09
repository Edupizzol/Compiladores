// erro de tipo (issue #44): operadores inteiros com float/double
int main() {
    float f = 1.5;
    double d = 2.0;
    int i = 3;
    int a = f % 2;
    int b = i << d;
    int c = f & i;
    int e = i % 2;
    return a + b + c + e;
}
