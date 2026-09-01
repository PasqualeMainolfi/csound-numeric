<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csndist.csd
;
; Order 1 is Manhattan, order 2 Euclidean. It is csnnorm of the difference, in
; one call.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr      = csnfromarray(array(1, 2, 3))
    b:CsnArr      = csnfromarray(array(4, 5, 6))

    manhattan:i   = csndist(a, b)
    euclid:i      = csndist(a, b, 2)
    prints("order 1 = %g, order 2 = %.4f\n", manhattan, euclid)

    ; the same answer as a norm of the difference
    diff:CsnArr   = csnsubtract(a, b)
    by_norm:i     = csnnorm(diff, 2)
    prints("csnnorm of the difference = %.4f\n", by_norm)

    ; and the per-element distances, when the total is not what is wanted
    each:CsnArr   = csnpairdist(a, b)
    each_out:i[]  = csntoarray(each)
    prints("per element = %g %g %g\n", each_out[0], each_out[1], each_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
