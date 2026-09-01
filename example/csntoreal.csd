<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csntoreal.csd
;
; The imaginary lane is dropped, not folded in. csnabs is the one to use when
; the magnitude is what is meant.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    re:CsnArr     = csnfromarray(array(3, 0, -1))
    im:CsnArr     = csnfromarray(array(4, 2, 0))
    j:Complex     = init(0, 1, 0)
    z:CsnArr      = csnadd(csntocomplex(re), csnmul(csntocomplex(im), j))

    narrowed:CsnArr = csntoreal(z)
    itype:i       = csntype(narrowed)
    out:i[]       = csntoarray(narrowed)
    prints("itype = %d, real parts = %g %g %g\n", itype, out[0], out[1], out[2])

    ; the round trip through csntocomplex loses the imaginary lane
    back:CsnArr   = csntocomplex(narrowed)
    back_im:CsnArr = csnimag(back)
    back_out:i[]  = csntoarray(back_im)
    prints("imaginary after the round trip = %g %g %g\n", back_out[0], back_out[1], back_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
