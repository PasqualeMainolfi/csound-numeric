<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

; a complex array has no string form: csnnumtostr refuses it, at init and in
; the k form, and leaves nothing locked

instr 1
    Real:CsnArr = csnfromarray(array(1, 2))
    Complex:CsnArr = csntocomplex(Real)
    Result:CsnStr = csnnumtostr(Complex)
endin

instr 2
    kOne init 1
    Real:CsnArr = csnfromarray(array(1, 2))
    Complex:CsnArr = csntocomplex(Real)
    Result:CsnStr = csnnumtostr(Complex, kOne)
endin

instr 3
    Source:CsnArr = csnfromarray(array(0.5, 2))
    Result:CsnStr = csnnumtostr(Source)
    SValues[] = csntoarray(Result)
    SFirst = SValues[0]
    iCmp = strcmp(SFirst, "0.5")
    assert(iCmp == 0)
    prints("csnum conversions alive after every complex refusal\n")
endin
</CsInstruments>

<CsScore>
i1 0    0.01
i2 0.02 0.01
i3 0.04 0.01
</CsScore>
</CsoundSynthesizer>
