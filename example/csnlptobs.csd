<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnlptobs.csd
;
; Turns a lowpass prototype into a bandstop, as transfer-function coefficients.
; Same arguments and results as scipy.signal.lp2bs.
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

    ; notch out a band 0.5 rad/s wide around 3 rad/s: (s^2 + 9) / (s^2 + 0.5 s + 9)
    bt:CsnArr, at:CsnArr csnlptobs b, a, 3, 0.5
    csnprint(bt)
    csnprint(at)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
