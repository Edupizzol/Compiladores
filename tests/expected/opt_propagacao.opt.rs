fn main() -> i32 {
    let mut a: i32 = 3;
    let mut b: i32 = a + 4;
    let mut c: i32 = b;
    let mut d: i32 = c << 1;
    while (d > 0) {
    {
    d = d - b;
    }
    }
    return d + c;
}
