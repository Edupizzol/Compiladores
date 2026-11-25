func quadrado(n):
    m = n
    t0 = m * m
    return t0
endfunc

func main():
    a = 3
    t0 = a + 4
    b = t0
    t1 = b * 2
    t2 = t1 - a
    c = t2
    t3 = c / 0
    d = t3
    param b
    t4 = call quadrado, 1
    t5 = t4 + c
    e = t5
    i = 0
L0:
    t6 = i < 3
    ifFalse t6 goto L2
    t7 = e + i
    e = t7
L1:
    t8 = i + 1
    i = t8
    goto L0
L2:
    return b
endfunc
