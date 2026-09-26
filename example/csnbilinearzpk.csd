<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnbilinearzpk.csd
;
; Maps an analog filter to a digital one by the bilinear transform, in zeros-poles-gain form.
; Same arguments and results as scipy.signal.bilinear_zpk: the zeros and poles come
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

    ; to digital, at a sampling rate of 10 Hz
    zd:CsnArr, pd:CsnArr, igain csnbilinearzpk z, p, 2, 10
    csnprint(zd)
    csnprint(pd)
    prints("gain = %.6f\n", igain)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
