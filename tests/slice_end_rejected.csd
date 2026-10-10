<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* The slice syntax ends inclusive, as Csound's own slicearray: v[0:6] on six
   elements names an index past the last. Expected to raise "Slice end 6 is
   past the last index 5"; the ctest entry matches on that text, so this file
   is a failure case by design. */

instr 1
    A:CsnArr = csnarange(0, 6, 1)
    W:CsnArr = A[0:6]
endin
</CsInstruments>

<CsScore>
i 1 0 0.003
e
</CsScore>
</CsoundSynthesizer>
