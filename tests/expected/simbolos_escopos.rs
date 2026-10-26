fn main() -> i32 {
    let mut x: i32 = 1;
    let mut y: i32 = 2;
    {
    let mut x: i32 = 3;
    {
    let mut y: f64 = 4.000000;
    x = x + 1;
    }
    }
    {
    let mut i: i32 = 0;
        while (i < 3) {
    {
    let mut t: i32 = i;
    x = x + t;
    }
            i = i + 1;
        }
    }
    {
    let mut i: i32 = 10;
        while (i > 0) {
    {
    y = y + i;
    }
            i = i - 1;
        }
    }
    {}
    // return x + y;
}
