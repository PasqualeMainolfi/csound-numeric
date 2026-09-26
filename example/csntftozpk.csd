<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csntftozpk.csd
;
; A transfer function b(s) / a(s), coefficients highest degree first, turned
; into its zeros, poles and gain, as scipy.signal.tf2zpk does. The zeros are
; the roots of b, the poles the roots of a, the gain b[0] / a[0]. Leading zeros
; only lower the degree and are skipped.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; (2s^2 - 2) / (s^2 + 3s + 2) = 2 (s - 1)(s + 1) / ((s + 1)(s + 2))
    ib[] = fillarray(2, 0, -2)
    ia[] = fillarray(1, 3, 2)
    b:CsnArr = csnfromarray(ib)
    a:CsnArr = csnfromarray(ia)
    z:CsnArr, p:CsnArr, igain csntftozpk b, a
    csnprint(z)
    csnprint(p)
    prints("gain = %.6f\n", igain)

    ; an all-pole filter has no zeros: the zeros come back empty
    ib2[] = fillarray(1)
    b2:CsnArr = csnfromarray(ib2)
    z2:CsnArr, p2:CsnArr, igain2 csntftozpk b2, a
    csnprint(z2)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
