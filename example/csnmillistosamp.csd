<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnmillistosamp.csd
;
; Milliseconds into sample counts at a chosen rate: ms sr / 1000. The result is
; not rounded: a duration that does not fall on a sample boundary keeps its
; fraction, and rounding it is left to whoever needs an integer.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    one:i           = csnmillistosamp(100, 44100)
    prints("100 ms at 44100 : %g samples\n", one)

    other:i         = csnmillistosamp(100, 48000)
    prints("100 ms at 48000 : %g samples\n", other)

    ; a set of delay taps in milliseconds
    taps:CsnArr     = csnfromarray(array(7, 13, 29, 53))
    in_samples:CsnArr = csnmillistosamp(taps, 44100)
    csnprint in_samples

    ; nothing is rounded: 7 ms is not a whole number of samples at 44100
    rounded:CsnArr  = csnround(in_samples)
    csnprint rounded

    ; and back
    back:CsnArr     = csnsamptomillis(in_samples, 44100)
    csnprint back
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
