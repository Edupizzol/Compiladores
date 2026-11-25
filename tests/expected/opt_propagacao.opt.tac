func main():
    a = 3
    b = 7
    c = 7
    d = 14
L0:
    t0 = d > 0
    ifFalse t0 goto L1
    t1 = d - b
    d = t1
    goto L0
L1:
    t2 = d + c
    return t2
endfunc
