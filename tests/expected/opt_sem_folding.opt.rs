fn f() -> i32 {
    return 7;
}

fn main() -> i32 {
    let mut a: i32 = 5 / 0;
    let mut b: i32 = 5 % 0;
    let mut c: i32 = 2147483647 + 1;
    let mut d: i32 = 1 << 32;
    let mut e: i32 = f() * 0;
    let mut g: i32 = 0;
    return e + g;
}
