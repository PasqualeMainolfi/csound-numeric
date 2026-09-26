<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnroots.csd
;
; The roots of a polynomial given by its coefficients, highest degree first,
; as np.roots takes them. A polynomial of degree N-1 has N-1 roots, returned
; as a complex vector in no particular order, even when all of them are real.
;
; With an axis, every 1-D slice along it is a separate polynomial: a (3, 2)
; matrix read along axis 0 is two quadratics, and gives a (2, 2) matrix of
; roots.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; x^3 - 6x^2 + 11x - 6 = (x - 1)(x - 2)(x - 3)
    ic[] = fillarray(1, -6, 11, -6)
    coefs:CsnArr = csnfromarray(ic)
    roots:CsnArr = csnroots(coefs)
    csnprint(roots)

    ; x^2 + 1 has no real root: +i and -i
    iq[] = fillarray(1, 0, 1)
    quad:CsnArr = csnfromarray(iq)
    croots:CsnArr = csnroots(quad)
    csnprint(croots)

    ; two quadratics side by side, one per column: x^2 - 3x + 2 and x^2 - 4
    iflat[] = fillarray(1, 1, -3, 0, 2, -4)
    ishape[] = fillarray(3, 2)
    flat:CsnArr = csnfromarray(iflat)
    cols:CsnArr = csnreshape(flat, ishape)
    colroots:CsnArr = csnroots(cols, 0)
    csnprint(colroots)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
