<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnnmtoacn.csd
;
; The ACN channel index of an ambisonics component, from its order n and its
; degree m:
;
;   ACN = n*n + n + m        with 0 <= n and -n <= m <= n
;
; ACN is the ordering AmbiX uses: the components come out in the order
; (0,0), (1,-1), (1,0), (1,1), (2,-2) ... which is exactly 0, 1, 2, 3, 4 ...
; csnacntonm reads the pair back.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; the whole of first order, in ACN order
    i00 = csnnmtoacn(0,  0)
    i1m = csnnmtoacn(1, -1)
    i10 = csnnmtoacn(1,  0)
    i1p = csnnmtoacn(1,  1)
    prints("first order: (0,0)=%d (1,-1)=%d (1,0)=%d (1,1)=%d\n", i00, i1m, i10, i1p)

    ; the corners of second order
    i2m = csnnmtoacn(2, -2)
    i2p = csnnmtoacn(2,  2)
    prints("second order runs from %d to %d\n", i2m, i2p)

    ; and the pair comes back
    in, im csnacntonm i1p
    prints("ACN %d is order %d degree %d\n", i1p, in, im)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
