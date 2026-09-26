<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnzpktotf.csd
;
; Zeros, poles and gain turned back into the coefficients of a transfer
; function b / a, highest degree first, as scipy.signal.zpk2tf does:
; b = k * prod(x - z), a = prod(x - p). The coefficients are real when the
; complex roots come in conjugate pairs, and complex otherwise.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; zeros at +-1, poles at -1 and -2, gain 2
    iz[] = fillarray(1, -1)
    ip[] = fillarray(-1, -2)
    z:CsnArr = csnfromarray(iz)
    p:CsnArr = csnfromarray(ip)
    b:CsnArr, a:CsnArr csnzpktotf z, p, 2
    csnprint(b)     ; [2 0 -2]
    csnprint(a)     ; [1 3 2]

    ; the round trip through csntftozpk gives the same filter back
    z2:CsnArr, p2:CsnArr, igain csntftozpk b, a
    b2:CsnArr, a2:CsnArr csnzpktotf z2, p2, igain
    csnprint(b2)
    csnprint(a2)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
