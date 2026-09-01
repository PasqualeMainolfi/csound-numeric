<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnquantile.csd
;
; The same reduction as csnpercentile with the fraction stated from 0 to 1.
; Along an axis it answers per row or per column.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr  = csnfromarray(array(2, 4, 4, 4, 5, 5, 7, 9))

    q25:i        = csnquantile(data, 0.25)
    q50:i        = csnquantile(data, 0.5)
    q90:i        = csnquantile(data, 0.9)
    prints("q0.25=%g q0.5=%g q0.9=%.2f\n", q25, q50, q90)

    ; the same thing said in percent
    p25:i        = csnpercentile(data, 25)
    prints("csnpercentile(25) = %g\n", p25)

    ; per row of a matrix
    shape:i[]    = fillarray(2, 3)
    mat:CsnArr   = csnreshape(csnfromarray(array(1, 2, 3, 10, 20, 30)), shape)
    rows:CsnArr  = csnquantile(mat, 0.5, 1)
    rows_out:i[] = csntoarray(rows)
    prints("median per row = %g %g\n", rows_out[0], rows_out[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
