<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsetslice.csd
;
; csnsetslice fills exactly the range csngetslice would read. Interleaving two
; signals is the same call twice, once on each phase.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]   = fillarray(6)
    dest:CsnArr = csnzeros(shape)

    evens:CsnArr = csnfromarray(array(1, 2, 3))
    odds:CsnArr  = csnfromarray(array(-1, -2, -3))

    csnsetslice(dest, evens, 0, 0, 6, 2)
    csnsetslice(dest, odds,  0, 1, 6, 2)

    dest_out:i[] = csntoarray(dest)
    prints("interleaved = %g %g %g %g %g %g\n", dest_out[0], dest_out[1], dest_out[2], dest_out[3], dest_out[4], dest_out[5])

    ; a whole column of a matrix
    mat_shape:i[] = fillarray(3, 2)
    mat:CsnArr    = csnzeros(mat_shape)
    col_shape:i[] = fillarray(3, 1)
    col:CsnArr    = csnreshape(csnfromarray(array(7, 8, 9)), col_shape)
    csnsetslice(mat, col, 1, 0, 1, 1)
    mat_out:i[]   = csntoarray(csnflatten(mat))
    prints("column 0    = %g %g %g %g %g %g\n", mat_out[0], mat_out[1], mat_out[2], mat_out[3], mat_out[4], mat_out[5])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
