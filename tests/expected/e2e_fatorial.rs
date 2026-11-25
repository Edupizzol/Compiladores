fn fat(mut n: i32) -> i32 {
    if (n <= 1) {
    return 1;
    }
    return n * fat(n - 1);
}

fn fat_iter(mut n: i32) -> i32 {
    let mut r: i32 = 1;
    while (n > 1) {
    {
    r = r * n;
    n = n - 1;
    }
    }
    return r;
}

fn main() {
    let mut a: i32 = fat(5);
    let mut b: i32 = fat_iter(2 + 2);
    std::process::exit(a - b * (3 - 2));
}
