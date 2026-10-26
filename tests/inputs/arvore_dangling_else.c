// arvore_dangling_else.c (issue #20)
// o else tem que ficar amarrado ao if mais proximo
int main() {
    int a = 1;
    int b = 1;
    int x = 0;
    if (a)
        if (b)
            x = 1;
        else
            x = 2;
    return x;
}
