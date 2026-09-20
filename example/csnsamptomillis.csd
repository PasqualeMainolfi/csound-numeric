<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsamptomillis.csd
;
; Sample counts into milliseconds at a chosen rate: 1000 n / sr. The sample rate
; is an argument rather than the orchestra's, so a file recorded at one rate can
; be measured while the orchestra runs at another. Scalar and array forms.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; one value
    one:i           = csnsamptomillis(4410, 44100)
    prints("4410 sm at 44100: %g ms\n", one)

    ; the same count read at another rate
    other:i         = csnsamptomillis(4410, 48000)
    prints("4410 sm at 48000: %g ms\n", other)

    ; a whole set of onsets at once
    onsets:CsnArr   = csnfromarray(array(0, 4410, 11025, 22050, 44100))
    millis:CsnArr   = csnsamptomillis(onsets, 44100)
    csnprint millis

    ; and back, which is what csnmillistosamp is for
    back:CsnArr     = csnmillistosamp(millis, 44100)
    csnprint back

    ; a delay line length in milliseconds, from a length in samples
    tap:i           = csnsamptomillis(2205, 44100)
    prints("2205 samples    : %g ms\n", tap)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
