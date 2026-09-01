<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnones.csd
;
; csnones fills a shape with ones. Combined with csnmul it is the shortest way
; to a constant array, and it is the natural neutral element for csnprod.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]    = fillarray(5)
    ones:CsnArr  = csnones(shape)
    ones_out:i[] = csntoarray(ones)
    prints("ones    = %g %g %g %g %g\n", ones_out[0], ones_out[1], ones_out[2], ones_out[3], ones_out[4])

    ; a constant array of 0.25, without a second constructor
    gain:CsnArr  = csnmul(ones, 0.25)
    gain_out:i[] = csntoarray(gain)
    prints("scaled  = %g %g %g %g %g\n", gain_out[0], gain_out[1], gain_out[2], gain_out[3], gain_out[4])

    product:i    = csnprod(ones)
    prints("product = %g\n", product)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
