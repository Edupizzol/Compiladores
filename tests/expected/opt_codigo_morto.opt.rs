fn main() {
    let mut x: i32 = 0;
    {
    x = 2;
    }
    {
    let mut y: i32 = 5;
    x = x + y;
    }
    {
    let mut i: i32 = 0;
    }
    while (x < 100) {
    {
    x = x + 10;
    break;
    }
    }
    std::process::exit(x);
}
