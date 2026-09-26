<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnlptobpzpk.csd
;
; Turns a lowpass prototype into a bandpass, in zeros-poles-gain form.
; Same arguments and results as scipy.signal.lp2bp_zpk: the zeros and poles come
; back complex, the gain as a scalar.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; analog prototype: one zero at -3, poles at -1, -2, -4, gain 2
    iz[] = fillarray(-3)
    ip[] = fillarray(-1, -2, -4)
    z:CsnArr = csnfromarray(iz)
    p:CsnArr = csnfromarray(ip)

    ; a band 0.5 rad/s wide around 3 rad/s
    zb:CsnArr, pb:CsnArr, igain csnlptobpzpk z, p, 2, 3, 0.5
    csnprint(zb)
    csnprint(pb)
    prints("gain = %.6f\n", igain)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
