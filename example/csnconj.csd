<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnconj.csd
;
; The imaginary lane changes sign and nothing else does. Multiplying by the
; conjugate is how power is read off a spectrum.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    re:CsnArr    = csnfromarray(array(3, 0, -1))
    im:CsnArr    = csnfromarray(array(4, 2, 0))
    j:Complex    = init(0, 1, 0)
    z:CsnArr     = csnadd(csntocomplex(re), csnmul(csntocomplex(im), j))

    conj:CsnArr  = csnconj(z)
    conj_re:CsnArr = csnreal(conj)
    conj_im:CsnArr = csnimag(conj)
    re_out:i[]   = csntoarray(conj_re)
    im_out:i[]   = csntoarray(conj_im)
    prints("real unchanged = %g %g %g\n", re_out[0], re_out[1], re_out[2])
    prints("imag flipped   = %g %g %g\n", im_out[0], im_out[1], im_out[2])

    ; z times its conjugate is the squared magnitude, imaginary lane at zero
    power:CsnArr = csnmul(z, conj)
    power_re:CsnArr = csnreal(power)
    power_im:CsnArr = csnimag(power)
    p_out:i[]    = csntoarray(power_re)
    pi_out:i[]   = csntoarray(power_im)
    prints("power = %g %g %g (imaginary %g %g %g)\n", p_out[0], p_out[1], p_out[2], pi_out[0], pi_out[1], pi_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
