fn media(a: i32, b: f32) -> f32 {
    return ((a as f32) + b) / (2 as f32);
}

fn main() -> i32 {
    let mut i: i32 = 3;
    let mut f: f32 = 2.500000;
    let mut d: f64 = 1.000000;
    let mut c: char;
    let mut r1: i32 = (((i as f32) + f) as i32);
    let mut r2: f64 = (i as f64) * d + (f as f64);
    let mut r3: f32 = media(i, (i as f32));
    let mut r4: i32 = (f > (i as f32));
    let mut r5: i32 = i % 2 + (i << 1);
    f = (i as f32);
    i = ((f * (2 as f32)) as i32);
    return -i;
}
