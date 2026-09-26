<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnlptobp.csd
;
; Turns a lowpass prototype into a bandpass, as transfer-function coefficients.
; Same arguments and results as scipy.signal.lp2bp.
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

    ; a band 0.5 rad/s wide around 3 rad/s: 0.5 s / (s^2 + 0.5 s + 9)
    bt:CsnArr, at:CsnArr csnlptobp b, a, 3, 0.5
    csnprint(bt)
    csnprint(at)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
