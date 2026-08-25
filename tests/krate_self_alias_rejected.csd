<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* csnflip reads the source to fill a differently laid out destination, so the
   output slot cannot also be the input: the reallocation would drop the data it
   is about to read. Expected to raise
   "Input array N is also this opcode's own output" — the ctest entry matches on
   that text, so this file is a failure case by design. */

giValues[] = array(1, 2, 3, 4, 5, 6)
giShape[] = array(2, 3)

Flat@global:CsnArr = csnfromarray(giValues)
Source@global:CsnArr = csnreshape(Flat, giShape)
Flipped@global:CsnArr = csnflip(Source, -1)

instr 1
    kAxis = -1
    Flipped = csnflip(Flipped, kAxis)
endin
</CsInstruments>

<CsScore>
i 1 0 0.003
e
</CsScore>
</CsoundSynthesizer>
