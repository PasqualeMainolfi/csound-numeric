<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnnormalize.csd
;
; Order 1 makes the magnitudes sum to 1, which is what a weight vector wants.
; Order 2 makes the length 1, which is what a direction wants.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(1, 2, 3))

    weights:CsnArr = csnnormalize(vec)
    weights_out:i[] = csntoarray(weights)
    total:i       = csnsum(weights)
    prints("order 1 = %.4f %.4f %.4f, sum = %g\n", weights_out[0], weights_out[1], weights_out[2], total)

    unit:CsnArr   = csnnormalize(vec, -1, 2)
    unit_out:i[]  = csntoarray(unit)
    length:i      = csnnorm(unit, 2)
    prints("order 2 = %.4f %.4f %.4f, length = %g\n", unit_out[0], unit_out[1], unit_out[2], length)

    ; per row, so every row comes out the same length
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 40, 50, 60)), shape)
    rows:CsnArr   = csnnormalize(mat, 1, 2)
    row_norms:CsnArr = csnnorm(rows, 1, 2)
    row_out:i[]   = csntoarray(row_norms)
    prints("row lengths after = %g %g\n", row_out[0], row_out[1])

    ; in place
    csnnormalize(vec)
    now:i[]       = csntoarray(vec)
    prints("in place = %.4f %.4f %.4f\n", now[0], now[1], now[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
