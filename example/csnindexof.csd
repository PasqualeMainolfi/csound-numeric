<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnindexof.csd
;
; Find the first occurrence of one scalar. The result is one coordinate per
; source dimension; a missing value produces an empty array.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr     = csnfromarray(array(1, 5, 3, 5, 2))
    needle:i       = 5
    first:CsnArr   = csnindexof(vec, needle)
    first_out:i[]  = csntoarray(first)
    prints("first 5 in the vector is at index %g\n", first_out[0])

    ; A matrix result contains (row, column).
    shape:i[]      = fillarray(2, 3)
    mat:CsnArr     = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    cell:CsnArr    = csnindexof(mat, needle)
    cell_out:i[]   = csntoarray(cell)
    prints("first 5 in the matrix is at (%g, %g)\n", cell_out[0], cell_out[1])

    ; Nothing found is represented by an empty coordinate array.
    missing:i      = 99
    none:CsnArr    = csnindexof(mat, missing)
    prints("no match: size = %d\n", csnsize(none))
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
