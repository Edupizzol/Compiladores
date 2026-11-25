func f():
    return 7
endfunc

func main():
    t0 = 5 / 0
    a = t0
    t1 = 5 % 0
    b = t1
    t2 = 2147483647 + 1
    c = t2
    t3 = 1 << 32
    d = t3
    t4 = call f, 0
    t5 = t4 * 0
    e = t5
    g = 0
    t6 = t5 + 0
    return t6
endfunc
