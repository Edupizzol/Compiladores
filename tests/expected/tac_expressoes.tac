func soma(a, b):
    t0 = a + b
    return t0
endfunc

func media(x, y):
    t0 = x + y
    t1 = (float) 2
    t2 = t0 / t1
    return t2
endfunc

func main():
    a = 1
    b = 2
    c = 3
    t0 = b * c
    t1 = a + t0
    x = t1
    t2 = a - b
    t3 = - t2
    t4 = ! c
    t5 = t3 * t4
    y = t5
    t6 = (float) 1.5
    f = t6
    t7 = (float) a
    t8 = t7 + f
    g = t8
    t9 = (float) a
    t10 = f > t9
    h = t10
    t11 = b * 2
    param a
    param t11
    t12 = call soma, 2
    param b
    param c
    t13 = call soma, 2
    param 1
    param t13
    t14 = call soma, 2
    t15 = t12 + t14
    k = t15
    t16 = (float) a
    t17 = (float) b
    param t16
    param t17
    t18 = call media, 2
    m = t18
    t19 = (int) ch
    t20 = t19 + 1
    z = t20
    t21 = x << 2
    t22 = y & 7
    t23 = t21 | t22
    return t23
endfunc
