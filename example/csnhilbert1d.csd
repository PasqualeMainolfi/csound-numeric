<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    source:CsnArr = csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8))
    analytic:CsnArr = csnhilbert1d(source)
    ; The real part is the source itself; the imaginary part is the transform.
    csnprint csnreal(analytic)
    csnprint csnimag(analytic)
    ; Amplitude envelope and instantaneous phase come straight off it.
    csnprint csnabs(analytic)
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
