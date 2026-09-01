<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csndiag.csd
;
; A matrix in gives its diagonal; a vector in gives a diagonal matrix. That is
; how a per-element gain becomes a linear map.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]     = fillarray(3, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8, 9)), shape)

    diag:CsnArr   = csndiag(mat)
    diag_out:i[]  = csntoarray(diag)
    diag_dims:i   = csndims(diag)
    prints("diagonal (dims %d) = %g %g %g\n", diag_dims, diag_out[0], diag_out[1], diag_out[2])

    ; the other direction: a vector becomes a diagonal matrix
    gains:CsnArr  = csnfromarray(array(0.5, 1, 2))
    gain_map:CsnArr = csndiag(gains)
    map_shape:i[] = csnshape(gain_map)
    map_out:i[]   = csntoarray(csnflatten(gain_map))
    prints("gain map %g x %g, row 1 = %g %g %g\n", map_shape[0], map_shape[1], map_out[3], map_out[4], map_out[5])

    ; and it applies as a linear map
    scaled:CsnArr = csnmatmul(mat, gain_map)
    scaled_out:i[] = csntoarray(csnflatten(scaled))
    prints("scaled row 0 = %g %g %g\n", scaled_out[0], scaled_out[1], scaled_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
