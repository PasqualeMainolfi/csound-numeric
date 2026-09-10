<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csninv.csd
;
; The inverse of a matrix, and the check that says whether it is one: multiply
; it back and see how far from the identity the product lands.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[] = fillarray(3, 3)
    a:CsnArr  = csnreshape(csnfromarray(array(4, 3, 2, 1, 5, 7, 2, 2, 9)), shape)

    inv:CsnArr = csninv(a)
    csnprint inv

    prod:CsnArr = csnmatmul(a, inv)
    eye:CsnArr  = csnidentity(3)
    diff:CsnArr = csnsubtract(prod, eye)
    worst:i     = csnmax(csnabs(diff))
    prints("A * inv(A) - I: %.2g\n", worst)

    ; solving is the better way to apply an inverse: fewer operations and less
    ; rounding than forming inv(A) and multiplying by it
    bshape:i[] = fillarray(3, 1)
    b:CsnArr   = csnreshape(csnfromarray(array(1, 2, 3)), bshape)
    viasolve:CsnArr = csnsolve(a, b)
    viainv:CsnArr   = csnmatmul(inv, b)
    gap:i = csnmax(csnabs(csnsubtract(viasolve, viainv)))
    prints("solve versus inv-then-multiply: %.2g apart\n", gap)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
