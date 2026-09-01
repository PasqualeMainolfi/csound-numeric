<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csninterp.csd
;
; A breakpoint table given as two parallel arrays. The mode fills the gaps and
; the bounds argument decides what happens off the ends.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

cap@global:i[]      = fillarray(0)
curve@global:CsnArr = csnempty(cap)

instr 1
    xs:CsnArr  = csnfromarray(array(0, 1, 2, 3))
    ys:CsnArr  = csnfromarray(array(0, 10, 20, 30))

    ; the five modes, at the same position
    linear:i   = csninterp(1.5, xs, ys, 0, 1)
    nearest:i  = csninterp(1.5, xs, ys, 1, 1)
    previous:i = csninterp(1.5, xs, ys, 2, 1)
    next:i     = csninterp(1.5, xs, ys, 3, 1)
    cubic:i    = csninterp(1.5, xs, ys, 4, 1)
    prints("at 1.5: linear=%g nearest=%g previous=%g next=%g cubic=%g\n", linear, nearest, previous, next, cubic)

    ; and the boundary policies, off the end of the table
    clamped:i  = csninterp(9, xs, ys, 0, 1)
    filled:i   = csninterp(9, xs, ys, 0, 2, -1)
    extrap:i   = csninterp(9, xs, ys, 0, 3)
    prints("at 9:   clamp=%g fill=%g extrapolate=%g\n", clamped, filled, extrap)

    ; the array form runs at performance time
    positions:CsnArr = csnfromarray(array(0.5, 1.5, 2.5))
    curve = csninterp(positions, xs, ys, 0, 1)
endin

instr 2
    n:i        = csnsize(curve)
    out:i[]    = csntoarray(curve)
    prints("array form, n = %d : %g %g %g\n", n, out[0], out[1], out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0   0.5
i 2 0.2 0.1
</CsScore>
</CsoundSynthesizer>
