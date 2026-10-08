<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* The rate only rises: csni in place cannot make an array that k opcodes
   write i-rate again, since they would keep rewriting it at perf while the
   operators ignored it. Expected to raise "already k-rate"; the ctest entry
   matches on that text, so this file is a failure case by design. */

instr 1
    iShape[] = fillarray(2)
    K:CsnArr = csnzeros(iShape)
    csnk K
    csni K
endin
</CsInstruments>

<CsScore>
i 1 0 0.003
e
</CsScore>
</CsoundSynthesizer>
