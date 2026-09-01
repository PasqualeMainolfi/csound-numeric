<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnstd.csd
;
; Population standard deviation, divided by N. It is the square root of csnvar,
; in the units of the data itself.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(2, 4, 4, 4, 5, 5, 7, 9))

    sd:i          = csnstd(data)
    variance:i    = csnvar(data)
    avg:i         = csnmean(data)
    prints("mean = %g, std = %g, var = %g\n", avg, sd, variance)

    ; the relation between the two
    check:i       = sd * sd
    prints("std squared = %g\n", check)

    ; along an axis
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 10, 20, 30)), shape)
    rows:CsnArr   = csnstd(mat, 1)
    rows_out:i[]  = csntoarray(rows)
    prints("per row = %.4f %.4f\n", rows_out[0], rows_out[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
