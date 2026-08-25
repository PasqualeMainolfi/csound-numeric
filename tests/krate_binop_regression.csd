<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* k-rate binary ops: handle-handle, handle-scalar, scalar-handle, the complex
   scalar overloads and the logical pair. Also covers the two things the k forms
   own on top of the i-rate ones: the trigger gate, and the fact that the result
   is republished into the slot the init pass registered instead of a freshly
   created one. */

giA[] = array(1, 2, 3, 4)
giB[] = array(10, 20, 30, 40)
giBase[] = array(2, 2, 2, 2)
giPowers[] = array(2, 4, 8, 16)

A@global:CsnArr = csnfromarray(giA)
B@global:CsnArr = csnfromarray(giB)
Base@global:CsnArr = csnfromarray(giBase)
Powers@global:CsnArr = csnfromarray(giPowers)

Sum@global:CsnArr = csnadd(A, B)
Diff@global:CsnArr = csnsubtract(A, B)
Scaled@global:CsnArr = csnmul(A, 2)
SubSH@global:CsnArr = csnsubtract(100, A)
DivSH@global:CsnArr = csndiv(120, A)
Powed@global:CsnArr = csnpow(A, 2)
LogAnd@global:CsnArr = csnlogicand(A, B)
LogOr@global:CsnArr = csnlogicor(A, B)
Logged@global:CsnArr = csnlog(Powers, Base)
Comp@global:CsnArr = csnfromarray(giA)
CompAdd@global:CsnArr = csnadd(A, 0)

/* Self-assignment is legal for an elementwise binop as long as the result keeps
   the aliased operand's layout: this one accumulates one A per control period. */
Acc@global:CsnArr = csnfromarray(giA)
Frozen@global:CsnArr = csnmul(A, 5)

instr 1
    kTrig init 1
    kOff init 0
    kTwo init 2
    kHundred init 100
    kOneTwenty init 120

    Sum = csnadd(A, B, kTrig)
    Diff = csnsubtract(A, B, kTrig)
    Scaled = csnmul(A, kTwo, kTrig)
    SubSH = csnsubtract(kHundred, A, kTrig)
    DivSH = csndiv(kOneTwenty, A, kTrig)
    Powed = csnpow(A, kTwo, kTrig)
    LogAnd = csnlogicand(A, B, kTrig)
    LogOr = csnlogicor(A, B, kTrig)
    Logged = csnlog(Powers, Base, kTrig)
    Acc = csnadd(Acc, A, kTrig)

    /* Gated off for the whole note: keeps whatever the init pass published,
       which is a copy of the array operand, not the product. */
    Frozen = csnmul(A, kTwo, kOff)
endin

instr 2
    i0[] = array(0)
    i3[] = array(3)

    iSum = csnget(Sum, i0)
    iDiff = csnget(Diff, i0)
    iScaled = csnget(Scaled, i0)
    iSubSH = csnget(SubSH, i0)
    iDivSH = csnget(DivSH, i0)
    iPowed = csnget(Powed, i3)
    iAnd = csnget(LogAnd, i0)
    iOr = csnget(LogOr, i0)
    iFrozen = csnget(Frozen, i0)
    iAcc = csnget(Acc, i0)
    iLogged = csnget(Logged, i3)

    assert(iSum == 11 && iDiff == -9)
    assert(iScaled == 2 && iPowed == 16)
    assert(iSubSH == 99 && iDivSH == 120)
    assert(iAnd == 1 && iOr == 1)
    /* log base 2 of 16: the hh form of csnlog wires its own init helper, so it
       needs a case of its own. */
    assert(iLogged == 4)
    /* Never triggered, so the k init's copy of A stands: 1, not 1 * 2. */
    assert(iFrozen == 1)

    /* Six control periods elapsed before this note: 1 + 6 * 1. */
    assert(iAcc == 7)

    /* Shapes and sizes survive the repeated republishing. */
    iSumShape[] = csnshape(Sum)
    assert(lenarray(iSumShape) == 1 && iSumShape[0] == 4)
    assert(csnsize(Sum) == 4 && csnsize(Acc) == 4)
endin
</CsInstruments>

<CsScore>
i 1 0 0.005
i 2 0.004 0.001
e
</CsScore>
</CsoundSynthesizer>
