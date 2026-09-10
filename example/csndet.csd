<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csndet.csd
;
; The determinant, real and complex. Zero is the answer for a singular matrix,
; not an error, which is what makes this the way to ask whether a matrix can be
; inverted at all.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[] = fillarray(3, 3)
    a:CsnArr  = csnreshape(csnfromarray(array(4, 3, 2, 1, 5, 7, 2, 2, 9)), shape)
    d:i       = csndet(a)
    prints("det = %g\n", d)

    ; row 3 is row 1 plus row 2: singular, so the determinant is zero
    dep:CsnArr = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6, 5, 7, 9)), shape)
    d_dep:i    = csndet(dep)
    prints("det of a dependent matrix = %g\n", d_dep)

    ; a complex matrix answers with a complex determinant
    j:Complex  = init(0, 1, 0)
    re:CsnArr  = csnreshape(csnfromarray(array(4, 3, 2, 1, 5, 7, 2, 2, 9)), shape)
    im:CsnArr  = csnreshape(csnfromarray(array(1, 0, 2, 0, 3, 1, 1, 1, 0)), shape)
    z:CsnArr   = csnadd(csntocomplex(re), csnmul(csntocomplex(im), j))
    D:Complex  = csndet(z)
    d_re:i     = real(D)
    d_im:i     = imag(D)
    prints("complex det = %g + %gi\n", d_re, d_im)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
