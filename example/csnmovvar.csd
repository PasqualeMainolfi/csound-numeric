<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnmovvar.csd
;
; Local spread, squared. csnmovstd is the same measure back in the units of the
; data.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr     = csnfromarray(array(1, 1, 1, 8, 1))

    smoothed:CsnArr = csnmovvar(data, 3)
    smoothed_out:i[] = csntoarray(smoothed)
    n:i             = csnsize(smoothed)
    prints("window 3, n = %d : %.4f %.4f %.4f %.4f %.4f\n", n, smoothed_out[0], smoothed_out[1], smoothed_out[2], smoothed_out[3], smoothed_out[4])

    ; a wider window
    wider:CsnArr    = csnmovvar(data, 5)
    wider_out:i[]   = csntoarray(wider)
    prints("window 5        : %.4f %.4f %.4f %.4f %.4f\n", wider_out[0], wider_out[1], wider_out[2], wider_out[3], wider_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
