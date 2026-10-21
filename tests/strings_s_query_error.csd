<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

; chr takes an ASCII code (1 to 127, whole), ord a one-byte string, and
; fromascii a real 1-D array of ASCII codes; a search range is checked as on
; an array. Each refusal leaves nothing locked.

instr 1
    SOut = csnstrchr(0)
endin

instr 2
    SOut = csnstrchr(65.5)
endin

instr 3
    iCode = csnstrord("ab")
endin

instr 4
    Codes:CsnArr = csnfromarray(array(72, 200))
    SOut = csnstrfromascii(Codes)
endin

instr 5
    Real:CsnArr = csnfromarray(array(72, 105))
    Codes:CsnArr = csntocomplex(Real)
    SOut = csnstrfromascii(Codes)
endin

instr 6
    iWhere = csnstrfind("abc", "b", 2, 1)
endin

instr 7
    Codes:CsnArr = csnfromarray(array(79, 75))
    SOut = csnstrfromascii(Codes)
    iCmp = strcmp(SOut, "OK")
    assert(iCmp == 0)
    prints("csnum S queries alive after every refusal\n")
endin
</CsInstruments>

<CsScore>
i1 0    0.01
i2 0.02 0.01
i3 0.04 0.01
i4 0.06 0.01
i5 0.08 0.01
i6 0.10 0.01
i7 0.12 0.01
</CsScore>
</CsoundSynthesizer>
