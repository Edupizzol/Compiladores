func main():
    x = 0
    x = 2
    y = 5
    x = 7
    i = 0
L0:
    t0 = x < 100
    ifFalse t0 goto L1
    t1 = x + 10
    x = t1
    goto L1
    goto L0
L1:
    return x
endfunc
