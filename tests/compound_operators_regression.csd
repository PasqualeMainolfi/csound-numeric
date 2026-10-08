<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

giA[] = fillarray(2, 4, 6)
giB[] = fillarray(1, 2, 3)
A@global:CsnArr = csnfromarray(giA)
B@global:CsnArr = csnfromarray(giB)

instr 1
    ; An i-rate target changes during init and must not change again at perf.
    Add:CsnArr = A
    Add += B
    iAdd[] = csntoarray(Add)
    assert(iAdd[0] == 3 && iAdd[1] == 6 && iAdd[2] == 9)

    Sub:CsnArr = A
    Sub -= B
    iSub[] = csntoarray(Sub)
    assert(iSub[0] == 1 && iSub[1] == 2 && iSub[2] == 3)

    Mul:CsnArr = A
    Mul *= 2
    iMul[] = csntoarray(Mul)
    assert(iMul[0] == 4 && iMul[1] == 8 && iMul[2] == 12)

    Div:CsnArr = A
    Div /= 2
    iDiv[] = csntoarray(Div)
    assert(iDiv[0] == 1 && iDiv[1] == 2 && iDiv[2] == 3)

    ; Compound assignment keeps a separate copy of its left operand.
    iA[] = csntoarray(A)
    assert(iA[0] == 2 && iA[1] == 4 && iA[2] == 6)
endin

Acc@global:CsnArr = csnfromarray(giA)
instr 2
    kOne init 1
    Acc += kOne
endin

instr 3
    iAcc[] = csntoarray(Acc)
    ; Six control periods have elapsed: 2 + 6 * 1.
    assert(iAcc[0] == 8 && iAcc[1] == 10 && iAcc[2] == 12)
endin

gkInlineFirst init 0
instr 4
    ; An inline array() argument selects csnfromarray's k-rate overload.
    ; The compound operation therefore first changes X at perf, after an
    ; init-time csnprint(X) would already have run.
    X:CsnArr = csnfromarray(array(2, 4, 6))
    Y:CsnArr = csnfromarray(array(1, 2, 3))
    X += Y
    kValues[] = csntoarray(X)
    gkInlineFirst = kValues[0]
    turnoff
endin

instr 5
    assert(i(gkInlineFirst) == 3)
endin
</CsInstruments>
<CsScore>
i 1 0 0.001
i 2 0.001 0.005
i 3 0.005 0.001
i 4 0.007 0.001
i 5 0.008 0.001
e
</CsScore>
</CsoundSynthesizer>
