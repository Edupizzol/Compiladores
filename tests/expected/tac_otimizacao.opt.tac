func quadrado(n):
    m = n
    t0 = n * n
    return t0
endfunc

func main():
    a = 3
    b = 7
    c = 11
    t0 = 11 / 0
    d = t0
    param 7
    t1 = call quadrado, 1
    t2 = t1 + c
    e = t2
    i = 0
L0:
    t3 = i < 3
    ifFalse t3 goto L1
    t4 = e + i
    e = t4
    t5 = i + 1
    i = t5
    goto L0
L1:
    return b
endfunc
