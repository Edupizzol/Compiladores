fn main() {
    let mut n: i32 = 3;
    let mut soma: i32 = 0;
    let mut x: f64 = 0.500000;
    while (n != 0) {
    {
    soma = soma + n;
    n = n - 1;
    }
    }
    if (x != 0.0) {
    {
    soma = soma + 10;
    }
    }
    {
    let mut i: i32 = 4;
        while (i != 0) {
    {
    if (i % 2 != 0) && (soma != 0) {
    {
    soma = soma + 1;
    }
    }
    }
            i = i - 1;
        }
    }
    if (x == 0.0) || (n != 0) {
    {
    soma = 0;
    }
    }
    std::process::exit(soma);
}
