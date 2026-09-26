<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnlptohp.csd
;
; Turns a lowpass prototype into a highpass, as transfer-function coefficients.
; Same arguments and results as scipy.signal.lp2hp.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; first-order lowpass prototype 1 / (s + 1)
    ib[] = fillarray(1)
    ia[] = fillarray(1, 1)
    b:CsnArr = csnfromarray(ib)
    a:CsnArr = csnfromarray(ia)

    ; a highpass with its cutoff at 3 rad/s: s / (s + 3)
    bt:CsnArr, at:CsnArr csnlptohp b, a, 3
    csnprint(bt)
    csnprint(at)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
