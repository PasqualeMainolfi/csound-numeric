<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnpad.csd
;
; Without an axis every axis grows, which frames a matrix on all four sides.
; With an axis, only that one does.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr      = csnfromarray(array(1, 2, 3, 4))
    padded:CsnArr   = csnpad(vec, 1, 2, -1)
    padded_out:i[]  = csntoarray(padded)
    n:i             = csnsize(padded)
    prints("n = %d, values = %g %g %g %g %g %g %g\n", n, padded_out[0], padded_out[1], padded_out[2], padded_out[3], padded_out[4], padded_out[5], padded_out[6])

    shape:i[]       = fillarray(2, 3)
    mat:CsnArr      = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    frame:CsnArr    = csnpad(mat, 1, 1, 0)
    frame_shape:i[] = csnshape(frame)
    prints("every axis: %g x %g\n", frame_shape[0], frame_shape[1])

    rows:CsnArr     = csnpad(mat, 1, 1, 0, 0)
    rows_shape:i[]  = csnshape(rows)
    prints("axis 0 only: %g x %g\n", rows_shape[0], rows_shape[1])

    ; in place
    csnpad(vec, 0, 1, 9)
    now:i[]         = csntoarray(vec)
    now_n:i         = csnsize(vec)
    prints("in place n = %d, last = %g\n", now_n, now[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
