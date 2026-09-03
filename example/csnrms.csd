<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    block:CsnArr  = csnfromarray(array(3, 4))
    level:i       = csnrms(block)
    prints("level    = %.4f\n", level)

    shape:i[]     = fillarray(2, 3)
    flat:CsnArr   = csnfromarray(array(1, 2, 3, 4, 5, 6))
    mat:CsnArr    = csnreshape(flat, shape)

    rows:CsnArr   = csnrms(mat, 1)
    rows_out:i[]  = csntoarray(rows)
    prints("per row  = %.4f %.4f\n", rows_out[0], rows_out[1])

    cols:CsnArr   = csnrms(mat, 0)
    cols_out:i[]  = csntoarray(cols)
    prints("per col  = %.4f %.4f %.4f\n", cols_out[0], cols_out[1], cols_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
