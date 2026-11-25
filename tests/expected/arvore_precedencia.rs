fn f(a: i32, b: i32) -> i32 {
    return a;
}

fn g(a: i32) -> i32 {
    return a;
}

fn main() {
    let mut a: i32 = 1;
    let mut b: i32 = 2;
    let mut c: i32 = 3;
    let mut p1: i32 = a + b * c;
    let mut p2: i32 = a - b - c;
    let mut p3: i32 = -a * b;
    let mut p4: i32 = (((a == 0) && (b != 0)) as i32);
    let mut p5: i32 = f(a, g(b));
    std::process::exit(p1);
}
