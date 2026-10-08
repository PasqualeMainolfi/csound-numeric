<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* csndiff shortens the axis it runs along, so its output cannot be its own
   source: the new layout would be stamped over the data it is about to read.
   Expected to raise "cannot be written over one of its own operands"; the
   ctest entry matches on that text, so this file is a failure case by design.
   (Opcodes that keep the layout and read each cell before writing it, such as
   csnflip, csnsort or csnadd, do run in place.) */

giValues[] = array(1, 2, 3, 4, 5, 6)
Diffed@global:CsnArr = csnfromarray(giValues)

instr 1
    kTrig = 1
    Diffed = csndiff(Diffed, kTrig)
endin
</CsInstruments>

<CsScore>
i 1 0 0.003
e
</CsScore>
</CsoundSynthesizer>
