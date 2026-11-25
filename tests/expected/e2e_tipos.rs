fn media(mut a: i32, mut b: i32) -> f64 {
    return ((a + b) as f64) / 2.000000;
}

fn main() {
    let mut c: char = (97 as u8 as char);
    let mut x: i32 = 7 / 2;
    let mut m: f64 = media(3, 4);
    let mut t: i32 = ((m * (10 as f64)) as i32);
    let mut d: i32 = -7 / 2;
    let mut r: i32 = -7 % 3;
    std::process::exit((c as i32) + x + t + d + r);
}
