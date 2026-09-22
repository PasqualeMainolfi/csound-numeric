<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnlagrange.csd
;
; The Lagrange interpolating polynomial through N points: the unique
; polynomial of degree at most N-1 that passes through all of them. The result
; is its N coefficients, highest degree first, as scipy.interpolate.lagrange
; hands them to np.poly1d.
;
; The x values must be distinct. With many points the polynomial oscillates
; between them (Runge's phenomenon) and the coefficients lose precision: keep N
; small.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; four points on 2x^3 - x + 4
    ix[] = fillarray(-1, 0.5, 2, 3)
    iy[] = fillarray(3, 3.75, 18, 55)
    hx:CsnArr = csnfromarray(ix)
    hy:CsnArr = csnfromarray(iy)
    coefs:CsnArr = csnlagrange(hx, hy)
    ic[] = csntoarray(coefs)
    prints("coefficients, highest degree first: %.4f %.4f %.4f %.4f\n", ic[0], ic[1], ic[2], ic[3])

    ; evaluate it between the points, by Horner
    it = 1
    iv = ((ic[0] * it + ic[1]) * it + ic[2]) * it + ic[3]
    prints("p(1) = %.4f\n", iv)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
