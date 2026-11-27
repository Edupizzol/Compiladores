fn main() {
    let mut i: i32;
    let mut a: i32;
    let mut b: i32;
    let mut c: i32;
    let mut total: i32 = 0;
    if (i != 0) {
    if (a != 0) {
    total = 1;
    } else {
    total = 2;
    }
    }
    if (b != 0) {
    {
    total = 3;
    }
    } else {
    {
    total = 4;
    }
    }
    while (total != 0) {
    {
    total = total + 1;
    if (total != 0) {
    break;
    } else {
    continue;
    }
    }
    }
    {
    let mut j: i32 = 0;
        while (j != 0) {
    {
    total = total + j;
    }
            j = j + 1;
        }
    }
    {
        while true {
    {
    break;
    }
            
        }
    }
    std::process::exit(total);
}
