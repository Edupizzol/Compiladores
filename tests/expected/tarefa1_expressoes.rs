fn zero() -> i32 {
    // return 0;
}

fn soma(a: i32, b: i32) -> i32 {
    // return a + b;
}

fn media(x: f64, y: f64) -> f64 {
    // return (x + y) / 2.000000;
}

fn main() -> i32 {
    let mut a: i32 = 7;
    let mut b: i32 = 3;
    let mut r1: i32 = a + b * 2 - a / b % 2;
    let mut r2: i32 = a - b - 1;
    let mut r3: i32 = (a + b) * (a - b);
    let mut r4: i32 = -a + -(b * 2);
    let mut r5: i32 = !a;
    let mut r6: i32 = - -a;
    let mut r7: i32 = ((a < b) == (b > a));
    let mut r8: i32 = ((a <= b) != (a >= b));
    let mut r9: i32 = (a > 0) || (b > 0) && !(a == b);
    let mut r10: i32 = a & (b == 3);
    let mut r11: i32 = a | b ^ a & b;
    let mut r12: i32 = (a << 2 >> 1 < b);
    let mut c1: i32 = zero();
    let mut c2: i32 = soma(a, b * 2);
    let mut c3: i32 = soma(soma(a, 1), zero()) + 1;
    let mut m: f64 = media(1.500000, 2.500000) * 2.000000;
    // return (soma(r1, c3) > 10) && (a != b);
}
