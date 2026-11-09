func main():
    i = 0
    soma = 0
L0:
    t0 = i < 10
    ifFalse t0 goto L1
    t1 = i + 1
    i = t1
    t2 = i == 3
    ifFalse t2 goto L2
    goto L0
L2:
    t3 = i > 8
    ifFalse t3 goto L3
    goto L1
L3:
    t4 = soma + i
    soma = t4
    goto L0
L1:
    j = 0
L4:
    t5 = j < 5
    ifFalse t5 goto L6
    t8 = j % 2
    t9 = t8 == 0
    ifFalse t9 goto L11
    t10 = soma > 0
    ifFalse t10 goto L11
    t7 = 1
    goto L12
L11:
    t7 = 0
L12:
    if t7 goto L9
    t11 = j == 4
    if t11 goto L9
    t6 = 0
    goto L10
L9:
    t6 = 1
L10:
    ifFalse t6 goto L7
    t12 = soma - 1
    soma = t12
    goto L8
L7:
    t13 = soma + 2
    soma = t13
L8:
L5:
    t14 = j + 1
    j = t14
    goto L4
L6:
L13:
    goto L15
L14:
    goto L13
L15:
    return soma
endfunc
