func main():
    x = 1
    x.1 = 2
    t0 = x.1 + 1
    x.1 = t0
    d = 0.5
    param x
    t1 = call later, 1
    t2 = (double) t1
    t3 = t2 + d
    d = t3
    return x
endfunc

func later(p):
    x = p
    t0 = x * 2
    return t0
endfunc
