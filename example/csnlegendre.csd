<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnlegendre.csd
;
; The associated Legendre function P_n^m(x), for 0 <= m <= n and x in [-1, 1],
; WITHOUT the Condon-Shortley phase:
;
;   scipy.special.lpmv(m, n, x) == (-1)^m * csnlegendre(n, m, x)
;
; Note the argument order: degree n first, then order m, as the harmonics
; write it, not scipy's (m, n).
;
; One form is scalar, at init or k-rate; the other maps a handle elementwise
; and keeps its shape.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ix = 0.3
    prints("x = %.2f\n", ix)
    in = 0
    while in <= 3 do
        im = 0
        while im <= in do
            iP = csnlegendre(in, im, ix)
            prints("  P(%d, %d) = %+.6f\n", in, im, iP)
            im += 1
        od
        in += 1
    od

    ; P_2^1 is 3x sqrt(1 - x^2) here; scipy's lpmv(1, 2, x) is its negative
    iP21 = csnlegendre(2, 1, ix)
    iCs = -iP21
    prints("P(2, 1) = %.6f, with the Condon-Shortley phase %.6f\n", iP21, iCs)

    ; elementwise over a handle: P_3^0 on a grid over [-1, 1]
    grid:CsnArr = csnlinspace(-1, 1, 5)
    p30:CsnArr = csnlegendre(3, 0, grid)
    ig[] = csntoarray(grid)
    ip[] = csntoarray(p30)
    prints("P(3, 0) at %.1f %.1f %.1f %.1f %.1f\n", ig[0], ig[1], ig[2], ig[3], ig[4])
    prints("         = %.4f %.4f %.4f %.4f %.4f\n", ip[0], ip[1], ip[2], ip[3], ip[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
