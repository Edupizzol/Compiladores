fn soma(mut a: i32, mut b: i32) -> i32 {
    return a + b;
}

fn main() {
    let mut x: i32 = 1;
    let mut y: i32 = 2;
    let mut z: i32 = soma(soma(x, y), x);
    std::process::exit(z);
}
