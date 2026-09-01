<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnmovmean.csd
;
; A moving average is the simplest smoother. The window is centred and shrinks
; at the ends, so the length is preserved.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr     = csnfromarray(array(1, 5, 2, 8, 3))

    smoothed:CsnArr = csnmovmean(data, 3)
    smoothed_out:i[] = csntoarray(smoothed)
    n:i             = csnsize(smoothed)
    prints("window 3, n = %d : %.4f %.4f %.4f %.4f %.4f\n", n, smoothed_out[0], smoothed_out[1], smoothed_out[2], smoothed_out[3], smoothed_out[4])

    ; a wider window
    wider:CsnArr    = csnmovmean(data, 5)
    wider_out:i[]   = csntoarray(wider)
    prints("window 5        : %.4f %.4f %.4f %.4f %.4f\n", wider_out[0], wider_out[1], wider_out[2], wider_out[3], wider_out[4])

    ; In place reads the same pre-pass values and therefore matches smoothed.
    inplace:CsnArr = csncopy(data)
    csnmovmean(inplace, 3)
    inplace_out:i[] = csntoarray(inplace)
    prints("in place       : %.4f %.4f %.4f %.4f %.4f\n", inplace_out[0], inplace_out[1], inplace_out[2], inplace_out[3], inplace_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
