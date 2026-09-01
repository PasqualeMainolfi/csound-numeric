<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnlogicand.csd
;
; Two comparisons, one band: csnlogicand keeps only the elements that pass both
; tests, which is how a range filter is written.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(-2, 0.5, 1, 3, 5))

    above:CsnArr   = csngt(data, 0)
    below:CsnArr   = csnlt(data, 4)
    band:CsnArr    = csnlogicand(above, below)
    band_out:i[]   = csntoarray(band)
    prints("in (0, 4) = %g %g %g %g %g\n", band_out[0], band_out[1], band_out[2], band_out[3], band_out[4])

    inside:i       = csnsum(band)
    prints("count     = %g\n", inside)

    ; keep only what passed
    kept:CsnArr    = csnmul(data, band)
    kept_out:i[]   = csntoarray(kept)
    prints("kept      = %g %g %g %g %g\n", kept_out[0], kept_out[1], kept_out[2], kept_out[3], kept_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
