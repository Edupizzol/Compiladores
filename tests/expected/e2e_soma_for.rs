fn main() {
    let mut soma: i32 = 0;
    {
    let mut i: i32 = 1;
        while (i <= 100) {
    {
    if (i % 2 == 0) {
    i = i + 1;
    continue;
    }
    if (soma > 50) {
    break;
    }
    soma = soma + i;
    }
            i = i + 1;
        }
    }
    let mut pares: i32 = 0;
    {
    let mut j: i32 = 0;
        while (j < 10) {
    {
    let mut k: i32 = 0;
    while (k < 3) {
    {
    k = k + 1;
    if (k == 2) {
    continue;
    }
    pares = pares + 1;
    }
    }
    if (j == 1 * 4) {
    j = j + 1;
    continue;
    }
    pares = pares + 10;
    }
            j = j + 1;
        }
    }
    std::process::exit(soma + pares);
}
