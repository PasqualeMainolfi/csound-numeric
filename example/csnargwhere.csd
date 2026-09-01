<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnargwhere.csd
;
; One row per match, each row the full coordinates. On a vector the rows are one
; long, so the result reads as a plain list of indices.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(1, 5, 3, 5, 2))
    wanted:CsnArr = csnfromarray(array(5))

    hits:CsnArr   = csnargwhere(vec, wanted)
    hits_out:i[]  = csntoarray(csnflatten(hits))
    count:i       = csnsize(hits)
    prints("matches = %d, at indices %g and %g\n", count, hits_out[0], hits_out[1])

    ; two values at once, on a matrix: each row is (row, column)
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    targets:CsnArr = csnfromarray(array(2, 6))
    cells:CsnArr  = csnargwhere(mat, targets)
    cells_out:i[] = csntoarray(csnflatten(cells))
    prints("2 is at (%g, %g), 6 is at (%g, %g)\n", cells_out[0], cells_out[1], cells_out[2], cells_out[3])

    ; nothing matches: an empty result, not an error
    missing:CsnArr = csnfromarray(array(99))
    none:CsnArr   = csnargwhere(vec, missing)
    none_n:i      = csnsize(none)
    prints("no match: size = %d\n", none_n)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
