<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnall.csd
;
; Compare, then csnall: "is every element positive" is one line. With an axis it
; answers per row or per column instead.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(1, 2, 3, 4))
    positive:i     = csnall(csngt(data, 0))
    over_two:i     = csnall(csngt(data, 2))
    prints("all > 0 = %d, all > 2 = %d\n", positive, over_two)

    ; per row: the mask of a 2 x 3 reduced along axis 1
    shape:i[]      = fillarray(2, 3)
    mat:CsnArr     = csnreshape(csnfromarray(array(1, 2, 3, 0, 5, 6)), shape)
    mask:CsnArr    = csngt(mat, 0)
    rows:CsnArr    = csnall(mask, 1)
    rows_out:i[]   = csntoarray(rows)
    prints("row 0 all positive = %g, row 1 = %g\n", rows_out[0], rows_out[1])

    ; the neutral answer over nothing
    cap:i[]        = fillarray(4)
    nothing:CsnArr = csnempty(cap)
    over_empty:i   = csnall(nothing)
    prints("over an empty array = %d\n", over_empty)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
