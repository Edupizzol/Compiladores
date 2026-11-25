fn main() {
    let mut x: i32 = 3;
    let mut a: i32 = 2 + 3 * 4;
    let mut b: i32 = 5 + x * (2 + 2);
    let mut c: i32 = (1 + 2) * (3 + 4);
    std::process::exit(a + b + c);
}
