<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsolve.csd
;
; Solves A x = B for x. The right-hand side may carry several columns at once,
; and the answer is checked the way it should be: by multiplying it back.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]  = fillarray(3, 3)
    bshape:i[] = fillarray(3, 1)
    a:CsnArr   = csnreshape(csnfromarray(array(4, 3, 2, 1, 5, 7, 2, 2, 9)), shape)
    b:CsnArr   = csnreshape(csnfromarray(array(1, 2, 3)), bshape)

    x:CsnArr   = csnsolve(a, b)
    csnprint x

    ; A x should give B back
    back:CsnArr = csnmatmul(a, x)
    diff:CsnArr = csnsubtract(back, b)
    worst:i     = csnmax(csnabs(diff))
    prints("largest residual: %.2g\n", worst)

    ; two right-hand sides in one call, one column each
    b2shape:i[] = fillarray(3, 2)
    b2:CsnArr   = csnreshape(csnfromarray(array(1, 0,  2, 1,  3, 0)), b2shape)
    x2:CsnArr   = csnsolve(a, b2)
    x2_shape:i[] = csnshape(x2)
    prints("two columns in, %d x %d out\n", x2_shape[0], x2_shape[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
