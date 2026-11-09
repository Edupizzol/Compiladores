fn fatorial(n: i32) -> i32 {
    if (n <= 1) {
    return 1;
    }
    return n * fatorial(n - 1);
}

fn main() -> i32 {
    let mut r: i32 = fatorial(5);
    let mut m: f64 = media(1.000000, 2.000000);
    return r;
}

fn media(a: f64, b: f64) -> f64 {
    return (a + b) / 2.000000;
}

fn nada() -> () {
    let mut vazio: i32 = 0;
}
