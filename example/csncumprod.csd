<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csncumprod.csd
;
; A running product. Over an array of ratios it turns intervals into absolute
; frequencies, one multiplication per step.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr     = csnfromarray(array(1, 2, 3, 4))
    running:CsnArr  = csncumprod(data)
    running_out:i[] = csntoarray(running)
    prints("cumprod = %g %g %g %g\n", running_out[0], running_out[1], running_out[2], running_out[3])

    ; intervals to a scale: each ratio multiplies the one before
    ratio:CsnArr    = csnfromarray(array(220, 1.125, 1.125, 1.0535))
    freq:CsnArr     = csncumprod(ratio)
    freq_out:i[]    = csntoarray(freq)
    prints("scale   = %.2f %.2f %.2f %.2f\n", freq_out[0], freq_out[1], freq_out[2], freq_out[3])

    ; one zero zeroes everything after it
    holed:CsnArr    = csncumprod(csnfromarray(array(2, 3, 0, 5)))
    holed_out:i[]   = csntoarray(holed)
    prints("with a zero = %g %g %g %g\n", holed_out[0], holed_out[1], holed_out[2], holed_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
