<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnget.csd
;
; One coordinate per dimension, in order. The output type picks the overload: a
; complex array is read into a :Complex; and a real one into a number.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]   = fillarray(2, 3)
    mat:CsnArr  = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    cell:i[]    = fillarray(1, 2)
    value:i     = csnget(mat, cell)
    prints("mat[1][2] = %g\n", value)

    ; one coordinate for a vector
    vec:CsnArr  = csnfromarray(array(10, 20, 30))
    first:i[]   = fillarray(0)
    head:i      = csnget(vec, first)
    prints("vec[0]    = %g\n", head)

    ; a complex array reads back as a :Complex;
    cpx:CsnArr  = csntocomplex(vec)
    z:Complex   = csnget(cpx, first)
    z_re:i      = real(z)
    z_im:i      = imag(z)
    prints("cpx[0]    = %g%+gi\n", z_re, z_im)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
