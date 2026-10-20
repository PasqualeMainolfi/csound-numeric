<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

; a repeat count on one S is a whole number, named or as an operator, and a
; count whose result would not fit a string size is refused before anything
; is allocated

instr 1
    SOut = "ab" * 2.5
endin

instr 2
    kHalf init 2.5
    SOut = csnstrmul("ab", kHalf)
endin

instr 3
    SOut = csnstrmul("ab", 1e19)
endin

instr 4
    SOut = "ab" * 2
    SMore = csnstradd(SOut, "!")
    iCmp = strcmp(SMore, "abab!")
    assert(iCmp == 0)
    prints("csnum add and mul on S alive after every refusal\n")
endin
</CsInstruments>

<CsScore>
i1 0    0.01
i2 0.02 0.01
i3 0.04 0.01
i4 0.06 0.01
</CsScore>
</CsoundSynthesizer>
