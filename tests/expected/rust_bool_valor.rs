fn main() {
    let mut a: i32 = 3;
    let mut b: i32 = ((a < 5) as i32);
    let mut c: i32 = ((a == 3) as i32) + ((a != 3) as i32);
    let mut d: i32 = (((a > 1) && (b != 0)) as i32);
    let mut e: i32 = ((a == 0) as i32);
    let mut f: i32 = ((!(a < b)) as i32);
    let mut g: f64 = (((a >= 3) as i32) as f64) * 2.500000;
    let mut h: i32 = ((((a < b) as i32) == ((b < a) as i32)) as i32);
    std::process::exit(b + c + d + e + f + h);
}
