<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnreflect.csd
;
; The component along b flips sign, the rest is kept. Reflecting twice gives the
; original back.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr       = csnfromarray(array(3, 4, 0))
    b:CsnArr       = csnfromarray(array(1, 0, 0))

    mirror:CsnArr  = csnreflect(a, b)
    mirror_out:i[] = csntoarray(mirror)
    prints("reflected = %g %g %g\n", mirror_out[0], mirror_out[1], mirror_out[2])

    ; twice is the identity
    back:CsnArr    = csnreflect(mirror, b)
    back_out:i[]   = csntoarray(back)
    prints("twice     = %g %g %g\n", back_out[0], back_out[1], back_out[2])

    ; the length is preserved
    len_a:i        = csnnorm(a, 2)
    len_m:i        = csnnorm(mirror, 2)
    prints("lengths: %g and %g\n", len_a, len_m)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
