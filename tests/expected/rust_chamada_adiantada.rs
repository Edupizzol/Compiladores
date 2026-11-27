fn main() {
    let mut a: i32 = ((metade(7)) as i32);
    let mut b: f64 = metade(3) + (1 as f64);
    std::process::exit(a + positivo(b));
}

fn metade(mut v: i32) -> f64 {
    return (v as f64) / 2.000000;
}

fn positivo(mut x: f64) -> i32 {
    return ((x > (0 as f64)) as i32);
}
