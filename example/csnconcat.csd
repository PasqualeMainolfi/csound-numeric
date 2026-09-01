<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnconcat.csd
;
; Without an axis both operands are read flat, so ranks need not agree. With an
; axis they are stacked, and every other axis must match.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr        = csnfromarray(array(1, 2, 3))
    b:CsnArr        = csnfromarray(array(4, 5))

    joined:CsnArr   = csnconcat(a, b)
    joined_out:i[]  = csntoarray(joined)
    n:i             = csnsize(joined)
    prints("flat n = %d, values = %g %g %g %g %g\n", n, joined_out[0], joined_out[1], joined_out[2], joined_out[3], joined_out[4])

    ; stacked along an axis
    shape:i[]       = fillarray(2, 3)
    mat:CsnArr      = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    stacked:CsnArr  = csnconcat(mat, mat, 0)
    stacked_shape:i[] = csnshape(stacked)
    prints("axis 0: %g x %g\n", stacked_shape[0], stacked_shape[1])

    ; concatenating with an empty array gives back the other operand
    cap:i[]         = fillarray(4)
    nothing:CsnArr  = csnempty(cap)
    same:CsnArr     = csnconcat(a, nothing)
    same_n:i        = csnsize(same)
    prints("with an empty operand: n = %d\n", same_n)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
