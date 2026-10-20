<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

; a replace count on one S is a whole number, as on an array: the i form and
; the k form refuse 1.5 at init

instr 1
    SOut = csnstrreplace("a-b", "-", "+", 1.5)
endin

instr 2
    kOne init 1
    SOut = csnstrreplace("a-b", "-", "+", 1.5, kOne)
endin

instr 3
    SOut = csnstrreplace("a-b", "-", "+")
    iCmp = strcmp(SOut, "a+b")
    assert(iCmp == 0)
    prints("csnum replace on S alive after every refusal\n")
endin
</CsInstruments>

<CsScore>
i1 0    0.01
i2 0.02 0.01
i3 0.04 0.01
</CsScore>
</CsoundSynthesizer>
