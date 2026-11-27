fn fatorial(mut n: i32) -> i32 {
    if (n <= 1) {
    return 1;
    }
    return n * fatorial(n - 1);
}

fn main() {
    let mut r: i32 = fatorial(5);
    let mut m: f64 = media(1.000000, 2.000000);
    std::process::exit(r);
}

fn media(mut a: f64, mut b: f64) -> f64 {
    return (a + b) / 2.000000;
}

fn nada() -> () {
    let mut vazio: i32 = 0;
}
