fn fib(mut n: i32) -> i32 {
    if (n < 2) {
    return n;
    }
    return fib(n - 1) + fib(n - 2);
}

fn main() {
    let mut a: i32 = 0;
    let mut b: i32 = 1;
    {
    let mut i: i32 = 0;
        while (i < 12) {
    {
    let mut t: i32 = a + b;
    a = b;
    b = t;
    }
            i = i + 1;
        }
    }
    if (a != fib(12)) {
    std::process::exit(1);
    }
    std::process::exit(a % 256);
}
