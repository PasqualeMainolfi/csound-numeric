<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsqrt.csd
;
; A real square root of a negative number is a NaN; the complex form returns the
; principal root instead.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(4, 9, 16, 25))

    roots:CsnArr   = csnsqrt(data)
    roots_out:i[]  = csntoarray(roots)
    prints("sqrt = %g %g %g %g\n", roots_out[0], roots_out[1], roots_out[2], roots_out[3])

    ; negatives give NaN over the reals
    mixed:CsnArr   = csnfromarray(array(4, -1, 9, -16))
    bad:CsnArr     = csnsqrt(mixed)
    nan_count:i    = csncntnan(bad)
    prints("NaN in the real root = %d\n", nan_count)

    ; over the complex field there is a principal root
    cpx:CsnArr     = csntocomplex(mixed)
    good:CsnArr    = csnsqrt(cpx)
    cell:i[]       = fillarray(1)
    z:Complex      = csnget(good, cell)
    z_re:i         = real(z)
    z_im:i         = imag(z)
    prints("sqrt(-1) = %g%+gi\n", z_re, z_im)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
