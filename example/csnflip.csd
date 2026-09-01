<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnflip.csd
;
; The axis decides what gets reversed: 0 turns a matrix upside down, 1 mirrors
; each row, -1 (the default) reverses the flat order.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr      = csnfromarray(array(1, 2, 3, 4))
    flat:CsnArr     = csnflip(vec)
    flat_out:i[]    = csntoarray(flat)
    prints("flat      = %g %g %g %g\n", flat_out[0], flat_out[1], flat_out[2], flat_out[3])

    shape:i[]       = fillarray(2, 3)
    mat:CsnArr      = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    by_rows:CsnArr  = csnflip(mat, 0)
    by_rows_out:i[] = csntoarray(csnflatten(by_rows))
    prints("axis 0    = %g %g %g %g %g %g\n", by_rows_out[0], by_rows_out[1], by_rows_out[2], by_rows_out[3], by_rows_out[4], by_rows_out[5])

    by_cols:CsnArr  = csnflip(mat, 1)
    by_cols_out:i[] = csntoarray(csnflatten(by_cols))
    prints("axis 1    = %g %g %g %g %g %g\n", by_cols_out[0], by_cols_out[1], by_cols_out[2], by_cols_out[3], by_cols_out[4], by_cols_out[5])

    ; in place
    csnflip(vec)
    now:i[]         = csntoarray(vec)
    prints("in place  = %g %g %g %g\n", now[0], now[1], now[2], now[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
