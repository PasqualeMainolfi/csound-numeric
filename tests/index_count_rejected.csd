<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* [] takes one index per dimension: a row of a 2-D array is not an item.
   The index arrives at perf time, through the k form. Expected to raise
   "Expected 2 indexes"; the ctest entry matches on that text, so this file is
   a failure case by design. */

giShape[] = fillarray(2, 3)

instr 1
    A:CsnArr = csnarange(0, 6, 1)
    M:CsnArr = csnreshape(A, giShape)
    kRow init 1
    kValue = M[kRow]
endin
</CsInstruments>

<CsScore>
i 1 0 0.003
e
</CsScore>
</CsoundSynthesizer>
