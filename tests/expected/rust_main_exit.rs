fn dobro(v: i32) -> i32 {
    return v * 2;
}

fn main() {
    let mut r: i32 = dobro(21);
    if (r > 40) {
    {
    std::process::exit(r);
    }
    }
    std::process::exit(0);
}
