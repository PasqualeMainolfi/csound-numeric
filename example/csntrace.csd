<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csntrace.csd
;
; The sum of the diagonal. Over the identity it is the order of the matrix, and
; csndiag is the same diagonal as an array.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]    = fillarray(3, 3)
    mat:CsnArr   = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8, 9)), shape)

    tr:i         = csntrace(mat)
    prints("trace = %g\n", tr)

    ; the same thing the long way
    diag:CsnArr  = csndiag(mat)
    diag_out:i[] = csntoarray(diag)
    by_sum:i     = csnsum(diag)
    prints("diagonal = %g %g %g, sum = %g\n", diag_out[0], diag_out[1], diag_out[2], by_sum)

    ; over the identity it is the order
    eye:CsnArr   = csnidentity(4)
    order:i      = csntrace(eye)
    prints("trace of the 4 x 4 identity = %g\n", order)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
