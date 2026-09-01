<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnmovmedian.csd
;
; A moving median deletes an isolated spike instead of smearing it, which is
; what separates it from a moving average.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr     = csnfromarray(array(1, 100, 2, 3, 4))

    smoothed:CsnArr = csnmovmedian(data, 3)
    smoothed_out:i[] = csntoarray(smoothed)
    n:i             = csnsize(smoothed)
    prints("window 3, n = %d : %g %g %g %g %g\n", n, smoothed_out[0], smoothed_out[1], smoothed_out[2], smoothed_out[3], smoothed_out[4])

    ; a wider window
    wider:CsnArr    = csnmovmedian(data, 5)
    wider_out:i[]   = csntoarray(wider)
    prints("window 5        : %g %g %g %g %g\n", wider_out[0], wider_out[1], wider_out[2], wider_out[3], wider_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
