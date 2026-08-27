<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* Most modes of the axis-wise unary helper may take their own output as input:
   the norm accumulates over a full read pass, the two sorts stage the slice
   through a scratch, the running totals write the cell they just read. csngrad
   is the exception — it writes the first cell and then reads it back as the
   left neighbour of the second — so it keeps the blanket rejection, and its
   layout being unchanged means nothing else would catch it.

   Expected to raise "Input array N is also this opcode's own output"; the ctest
   entry matches on that text, so this file is a failure case by design. */

giValues[] = array(1, 4, 9, 16)
Grad@global:CsnArr = csnfromarray(giValues)

instr 1
    kAxis init -1
    Grad = csngrad(Grad, kAxis)
endin
</CsInstruments>

<CsScore>
i 1 0 0.003
e
</CsScore>
</CsoundSynthesizer>
