<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* Array-by-array division refuses a zero divisor; division by a scalar zero
   follows IEEE instead. Both operands here are real and of the same shape, the
   dense path of the elementwise core, and the zero arrives at perf time.
   Expected to raise "Division by zero"; the ctest entry matches on that text,
   so this file is a failure case by design. */

instr 1
    iNum[] = fillarray(1, 2, 3, 4)
    iDen[] = fillarray(1, 2, 3, 4)
    Num:CsnArr = csnfromarray(iNum)
    Den:CsnArr = csnfromarray(iDen)
    kIndex[] init 1
    kIndex[0] = 2
    kZero = (timeinstk() >= 2 ? 0 : 3)
    csnset(Den, kIndex, kZero)
    Num /= Den
endin
</CsInstruments>

<CsScore>
i 1 0 0.003
e
</CsScore>
</CsoundSynthesizer>
