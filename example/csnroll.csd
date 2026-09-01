<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnroll.csd
;
; csnroll shifts and wraps: nothing is lost and the shape is unchanged. Without
; an axis the array is read flat; with one, every line along it moves on its own.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr     = csnfromarray(array(1, 2, 3, 4))

    right:CsnArr   = csnroll(vec, 1)
    right_out:i[]  = csntoarray(right)
    prints("shift  1 = %g %g %g %g\n", right_out[0], right_out[1], right_out[2], right_out[3])

    left:CsnArr    = csnroll(vec, -1)
    left_out:i[]   = csntoarray(left)
    prints("shift -1 = %g %g %g %g\n", left_out[0], left_out[1], left_out[2], left_out[3])

    ; along an axis, each row moves on its own
    shape:i[]      = fillarray(2, 3)
    mat:CsnArr     = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    rolled:CsnArr  = csnroll(mat, 1, 1)
    rolled_out:i[] = csntoarray(csnflatten(rolled))
    prints("axis 1   = %g %g %g %g %g %g\n", rolled_out[0], rolled_out[1], rolled_out[2], rolled_out[3], rolled_out[4], rolled_out[5])

    ; in place
    csnroll(vec, 2)
    now:i[]        = csntoarray(vec)
    prints("in place = %g %g %g %g\n", now[0], now[1], now[2], now[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
