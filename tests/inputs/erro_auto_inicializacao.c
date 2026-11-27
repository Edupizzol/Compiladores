// `int x = x;` (issue #55): em C o x da direita ja e a variavel nova, que
// ainda nao tem valor. Vale tambem com um x de fora visivel
int main() {
    int x = 5;
    {
        int x = x + 1;
    }
    int y = y;
    int z = x;
    return z;
}
