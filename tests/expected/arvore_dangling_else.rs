fn main() {
    let mut a: i32 = 1;
    let mut b: i32 = 1;
    let mut x: i32 = 0;
    if (a != 0) {
    if (b != 0) {
    x = 1;
    } else {
    x = 2;
    }
    }
    std::process::exit(x);
}
