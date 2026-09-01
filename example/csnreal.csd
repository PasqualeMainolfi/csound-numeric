<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnreal.csd
;
; csnreal and csnimag take a complex array apart into the two real arrays it
; was built from.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    re:CsnArr    = csnfromarray(array(3, 0, -1))
    im:CsnArr    = csnfromarray(array(4, 2, 0))
    j:Complex    = init(0, 1, 0)
    z:CsnArr     = csnadd(csntocomplex(re), csnmul(csntocomplex(im), j))

    parts:CsnArr = csnreal(z)
    parts_out:i[] = csntoarray(parts)
    itype:i      = csntype(parts)
    prints("itype = %d, real parts = %g %g %g\n", itype, parts_out[0], parts_out[1], parts_out[2])

    ; the conjugate leaves the real parts alone
    conj:CsnArr  = csnconj(z)
    conj_re:CsnArr = csnreal(conj)
    conj_out:i[] = csntoarray(conj_re)
    prints("after conjugation = %g %g %g\n", conj_out[0], conj_out[1], conj_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
