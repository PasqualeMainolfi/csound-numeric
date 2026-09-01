<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csntocomplex.csd
;
; Real in, complex out, imaginary lane at zero. Building a full complex array
; from two real ones is this plus a multiply by i and an add.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    re:CsnArr    = csnfromarray(array(1, 0, -1, 0))
    im:CsnArr    = csnfromarray(array(0, 1, 0, -1))

    widened:CsnArr = csntocomplex(re)
    itype:i      = csntype(widened)
    size:i       = csnsize(widened)
    prints("itype = %d, size = %d\n", itype, size)

    ; re + i*im, the usual way to assemble a complex array from two real ones
    j:Complex    = init(0, 1, 0)
    scaled:CsnArr = csnmul(csntocomplex(im), j)
    z:CsnArr     = csnadd(widened, scaled)

    z_re:CsnArr  = csnreal(z)
    z_im:CsnArr  = csnimag(z)
    re_out:i[]   = csntoarray(z_re)
    im_out:i[]   = csntoarray(z_im)
    prints("real = %g %g %g %g\n", re_out[0], re_out[1], re_out[2], re_out[3])
    prints("imag = %g %g %g %g\n", im_out[0], im_out[1], im_out[2], im_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
