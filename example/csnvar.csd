<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnvar.csd
;
; Population variance, divided by N. The square of csnstd, in the square of the
; data's units.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(2, 4, 4, 4, 5, 5, 7, 9))

    variance:i    = csnvar(data)
    sd:i          = csnstd(data)
    prints("var = %g, std = %g\n", variance, sd)

    ; a constant array has no spread at all
    flat:CsnArr   = csnfull(fillarray(5), 3)
    flat_var:i    = csnvar(flat)
    prints("variance of a constant array = %g\n", flat_var)

    ; along an axis
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 10, 20, 30)), shape)
    cols:CsnArr   = csnvar(mat, 0)
    cols_out:i[]  = csntoarray(cols)
    prints("per column = %.2f %.2f %.2f\n", cols_out[0], cols_out[1], cols_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
