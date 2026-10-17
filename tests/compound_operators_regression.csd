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

gkCopyAcc init 0
gkStillAcc init 0
instr 6
    ; A k-form producer makes its output k-rate even when its source is
    ; i-rate: csncopy with a trigger used to copy the source's rate over it,
    ; and the compound operation then never ran at perf.
    kFirst init 1
    Kacc:CsnArr = csncopy(B, kFirst)
    kFirst = 0
    Kacc += B
    ; All operands i-rate: applied once at init, never again at perf.
    Iacc:CsnArr = A
    Iacc += B
    kK[] = csntoarray(Kacc)
    kI[] = csntoarray(Iacc)
    gkCopyAcc = kK[0]
    gkStillAcc = kI[0]
endin

instr 7
    ; Four passes: copied once (1), then + 1 on each pass.
    assert(i(gkCopyAcc) == 5)
    assert(i(gkStillAcc) == 3)
endin

instr 8
    ; The operators on CsnStr, at init: + joins elementwise, with an S on
    ; either side, and * repeats, with the count on either side, as Python's
    ; str does; the compound forms apply once to an i-rate target.
    SOps[] fillarray "ab", "c"
    iOps:CsnStr = csnfromarray(SOps)
    iJoin:CsnStr = iOps + iOps
    iPost:CsnStr = iOps + "!"
    iPre:CsnStr = "<" + iOps
    iRep:CsnStr = iOps * 3
    iRep2:CsnStr = 2 * iOps
    iAccS:CsnStr = csnfromarray(SOps)
    iAccS += "!"
    iAccS += iOps
    iAccS *= 2
    SJoin1 = iJoin[1]
    SPost0 = iPost[0]
    SPre1 = iPre[1]
    SRep0 = iRep[0]
    SRep21 = iRep2[1]
    SAccS0 = iAccS[0]
    SOps0 = iOps[0]
    iJoinCmp = strcmp(SJoin1, "cc")
    iPostCmp = strcmp(SPost0, "ab!")
    iPreCmp = strcmp(SPre1, "<c")
    iRepCmp = strcmp(SRep0, "ababab")
    iRep2Cmp = strcmp(SRep21, "cc")
    iAccSCmp = strcmp(SAccS0, "ab!abab!ab")
    iOpsCmp = strcmp(SOps0, "ab")
    assert(iJoinCmp == 0 && iPostCmp == 0 && iPreCmp == 0 && iRepCmp == 0 && iRep2Cmp == 0 && iAccSCmp == 0 && iOpsCmp == 0)
endin

gkStrBad init -1
instr 9
    ; Four passes. An S has no rate: one that changes at perf makes an
    ; operator on an i-rate array compute again, and X += S on an i-rate X
    ; appends once per change. A k-rate X appends, and doubles, on every pass,
    ; and a k count repeats again whenever it changes. The init pass runs
    ; both branches of the if, "p" last.
    kc init 0
    if kc % 2 == 1 then
        Sv strcpyk "q"
    else
        Sv strcpyk "p"
    endif
    SFix[] fillarray "ab"
    iFix:CsnStr = csnfromarray(SFix)
    kPlus:CsnStr = iFix + Sv
    kPre:CsnStr = Sv + iFix
    iOnce:CsnStr = csnfromarray(SFix)
    iOnce += Sv
    ; a target that changes at perf is marked k-rate, or the slice reading it
    ; keeps the value it had at init
    OnceView:CsnStr = iOnce[0:0]
    ; a k count on an i-rate target repeats it on every pass, as ix *= kn does
    iMulK:CsnStr = csnfromarray(SFix)
    kTwo init 2
    iMulK *= kTwo
    kGrow:CsnStr = csnfromarray(SFix)
    csnk(kGrow)
    kGrow += "."
    kDbl:CsnStr = csnfromarray(SFix)
    csnk(kDbl)
    kDbl *= 2
    kRep:CsnStr = 2 * kDbl
    kN init 1
    kN = kc + 1
    kTimes:CsnStr = iFix * kN
    if kc == 3 then
        kI0 init 0
        S1 = kPlus[kI0]
        kBad = strcmpk(S1, "abq") != 0 ? 1 : 0
        S2 = kPre[kI0]
        kBad += strcmpk(S2, "qab") != 0 ? 1 : 0
        S3 = iOnce[kI0]
        kBad += strcmpk(S3, "abpqpq") != 0 ? 1 : 0
        S4 = kGrow[kI0]
        kBad += strcmpk(S4, "ab....") != 0 ? 1 : 0
        S5 = kDbl[kI0]
        kBad += strlenk(S5) != 32 ? 1 : 0
        S6 = kRep[kI0]
        kBad += strlenk(S6) != 64 ? 1 : 0
        S7 = kTimes[kI0]
        kBad += strcmpk(S7, "abababab") != 0 ? 1 : 0
        S8 = OnceView[kI0]
        kBad += strcmpk(S8, "abpqpq") != 0 ? 1 : 0
        S9 = iMulK[kI0]
        kBad += strlenk(S9) != 32 ? 1 : 0
        ; the init pass runs this branch too, with kc still 0
        gkStrBad = kc == 3 ? kBad : -1
    endif
    kc += 1
endin

instr 10
    assert(i(gkStrBad) == 0)
endin
</CsInstruments>
<CsScore>
i 1 0 0.001
i 2 0.001 0.005
i 3 0.005 0.001
i 4 0.007 0.001
i 5 0.008 0.001
i 6 0.01 [4 * 32 / 48000]
i 7 0.02 0.001
i 8 0.021 0.001
i 9 0.03 [4 * 32 / 48000]
i 10 0.04 0.001
e
</CsScore>
</CsoundSynthesizer>
