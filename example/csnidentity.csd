<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnidentity.csd
;
; csnidentity builds the n x n identity. Multiplying by it leaves a matrix
; alone, which is the quickest way to check a matmul chain.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    eye:CsnArr   = csnidentity(3)
    eye_out:i[]  = csntoarray(csnflatten(eye))
    prints("row 0 = %g %g %g\n", eye_out[0], eye_out[1], eye_out[2])
    prints("row 1 = %g %g %g\n", eye_out[3], eye_out[4], eye_out[5])
    prints("row 2 = %g %g %g\n", eye_out[6], eye_out[7], eye_out[8])

    tr:i         = csntrace(eye)
    prints("trace = %g\n", tr)

    ; the identity leaves a matrix alone
    shape:i[]    = fillarray(3, 3)
    mat:CsnArr   = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8, 9)), shape)
    same:CsnArr  = csnmatmul(mat, eye)
    same_out:i[] = csntoarray(csnflatten(same))
    prints("mat * I row 2 = %g %g %g\n", same_out[6], same_out[7], same_out[8])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
