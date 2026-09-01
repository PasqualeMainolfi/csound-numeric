<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnlogicor.csd
;
; The union of two masks. With csnlogicnot in front of an and, it is also how
; "outside the band" is written.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr      = csnfromarray(array(-2, 0.5, 1, 3, 5))

    very_low:CsnArr  = csnlt(data, 0)
    very_high:CsnArr = csngt(data, 4)
    extremes:CsnArr  = csnlogicor(very_low, very_high)
    extremes_out:i[] = csntoarray(extremes)
    prints("outside [0, 4] = %g %g %g %g %g\n", extremes_out[0], extremes_out[1], extremes_out[2], extremes_out[3], extremes_out[4])

    ; the same set, said as the complement of an and
    above:CsnArr     = csnge(data, 0)
    below:CsnArr     = csnle(data, 4)
    inside:CsnArr    = csnlogicand(above, below)
    outside:CsnArr   = csnlogicnot(inside)
    outside_out:i[]  = csntoarray(outside)
    prints("complement     = %g %g %g %g %g\n", outside_out[0], outside_out[1], outside_out[2], outside_out[3], outside_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
