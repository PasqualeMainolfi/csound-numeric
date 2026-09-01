<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnlogspace.csd
;
; csnlogspace spaces exponents evenly, then raises the base over them. Handy for
; a decade-wise frequency axis: the bounds are exponents, not frequencies.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    dec:CsnArr    = csnlogspace(1, 4, 4, 10)
    dec_out:i[]   = csntoarray(dec)
    prints("decades = %g %g %g %g\n", dec_out[0], dec_out[1], dec_out[2], dec_out[3])

    ; four octaves above 55 Hz, in base 2
    oct:CsnArr    = csnlogspace(0, 4, 5, 2)
    freq:CsnArr   = csnmul(oct, 55)
    freq_out:i[]  = csntoarray(freq)
    prints("octaves = %g %g %g %g %g\n", freq_out[0], freq_out[1], freq_out[2], freq_out[3], freq_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
