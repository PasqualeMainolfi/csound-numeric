<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnargisnan.csd
;
; No comparison can select a NaN, so this is the only way to find one. The real
; square root of a negative number is the easiest way to make some.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr  = csnfromarray(array(4, -1, 9, -16))
    roots:CsnArr = csnsqrt(data)

    bad:CsnArr   = csnargisnan(roots)
    bad_out:i[]  = csntoarray(csnflatten(bad))
    count:i      = csnsize(bad)
    prints("NaN count = %d, at %g and %g\n", count, bad_out[0], bad_out[1])

    ; equality cannot see them
    eq_hits:i    = csncnteq(roots, 0)
    nan_hits:i   = csncntnan(roots)
    prints("elements equal to 0 = %d, NaN elements = %d\n", eq_hits, nan_hits)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
