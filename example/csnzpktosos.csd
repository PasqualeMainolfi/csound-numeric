<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnzpktosos.csd
;
; A digital filter given as zeros, poles and gain, split into second-order
; sections, as scipy.signal.zpk2sos does with its default 'nearest' pairing.
; Each row is [b0 b1 b2 a0 a1 a2], a0 = 1; the gain sits in the first row and
; the poles closest to the unit circle in the last one. A cascade of sections
; keeps its precision at orders where a single b / a would lose it.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; three zeros at Nyquist, a real pole at 0.5 and the pair 0.3 +- 0.4j
    iz[] = fillarray(-1, -1, -1)
    ipr[] = fillarray(0.5, 0.3, 0.3)
    ipi[] = fillarray(0, 0.4, -0.4)
    z:CsnArr = csnfromarray(iz)
    pr:CsnArr = csnfromarray(ipr)
    pi:CsnArr = csnfromarray(ipi)
    prc:CsnArr = csntocomplex(pr)
    pic:CsnArr = csntocomplex(pi)
    j:Complex = init(0, 1, 0)
    pij:CsnArr = csnmul(pic, j)
    p:CsnArr = csnadd(prc, pij)

    ; an odd order is padded with a zero and a pole at the origin:
    ; [[1 2 1 1 -0.5 0]
    ;  [1 1 0 1 -0.6 0.25]]
    sos:CsnArr = csnzpktosos(z, p, 1)
    csnprint(sos)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
