<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnrandint.csd
;
; csnrandint draws integer-valued samples from [min, max). Both bounds may be
; negative, and seeding makes the draw reproducible.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    csnseed(12345)

    shape:i[]       = fillarray(8)
    values:CsnArr   = csnrandint(shape, -5, 0)
    rounded:CsnArr  = csnfloor(values)
    difference:CsnArr = csnsubtract(values, rounded)
    all_integer:i   = (csncnteq(difference, 0) == csnsize(values) ? 1 : 0)
    in_range:i      = (csnmin(values) >= -5 && csnmax(values) < 0 ? 1 : 0)

    csnprint(values)
    prints("all integer = %d, all inside [-5, 0) = %d\n", all_integer, in_range)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
