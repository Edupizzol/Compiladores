int main() {
    int i;
    int a, b, c;
    int total = 0;

    if (i)
        if (a)
            total = 1;
        else
            total = 2;

    if (b) {
        total = 3;
    } else {
        total = 4;
    }

    while (total) {
        total = total + 1;
        if (total)
            break;
        else
            continue;
    }

    for (int j = 0; j; j = j + 1) {
        total = total + j;
    }

    for (;;) {
        break;
    }

    return total;
}
