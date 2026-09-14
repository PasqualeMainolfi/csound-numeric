<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnflip.csd
;
; Omitting the axis reverses every axis. An explicit -1 reverses only the last
; axis, while -2 selects the penultimate one.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]       = fillarray(2, 3)
    mat:CsnArr      = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    every:CsnArr     = csnflip(mat)
    every_out:i[]    = csntoarray(csnflatten(every))
    prints("omitted   = %g %g %g %g %g %g\n", every_out[0], every_out[1], every_out[2], every_out[3], every_out[4], every_out[5])

    by_cols:CsnArr  = csnflip(mat, -1)
    by_cols_out:i[] = csntoarray(csnflatten(by_cols))
    prints("axis -1   = %g %g %g %g %g %g\n", by_cols_out[0], by_cols_out[1], by_cols_out[2], by_cols_out[3], by_cols_out[4], by_cols_out[5])

    by_rows:CsnArr  = csnflip(mat, -2)
    by_rows_out:i[] = csntoarray(csnflatten(by_rows))
    prints("axis -2   = %g %g %g %g %g %g\n", by_rows_out[0], by_rows_out[1], by_rows_out[2], by_rows_out[3], by_rows_out[4], by_rows_out[5])

    ; in place
    vec:CsnArr      = csnfromarray(array(1, 2, 3, 4))
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
