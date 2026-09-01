<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnargmin.csd
;
; The answer is a coordinate, not a value, and it is complete: a row of the
; result feeds straight back into csnget.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(3, 1, 9, 2))
    at:CsnArr     = csnargmin(vec)
    at_out:i[]    = csntoarray(csnflatten(at))
    smallest:i    = csnmin(vec)
    prints("smallest %g at index %g\n", smallest, at_out[0])

    ; on a matrix the row holds both coordinates
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(4, 9, 3, 1, 5, 6)), shape)
    cell:CsnArr   = csnargmin(mat)
    cell_out:i[]  = csntoarray(csnflatten(cell))
    prints("minimum at (%g, %g)\n", cell_out[0], cell_out[1])

    ; and it feeds straight back into csnget
    coord:i[]     = fillarray(cell_out[0], cell_out[1])
    value:i       = csnget(mat, coord)
    prints("value there = %g\n", value)

    ; one match per row, along an axis
    rows:CsnArr   = csnargmin(mat, 1)
    rows_out:i[]  = csntoarray(csnflatten(rows))
    prints("row 0 min at column %g, row 1 at column %g\n", rows_out[1], rows_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
