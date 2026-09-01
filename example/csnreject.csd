<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnreject.csd
;
; What is left of a once its component along b is taken out. Orthogonal to b by
; construction, so the scalar product with b is zero.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr       = csnfromarray(array(3, 4, 0))
    b:CsnArr       = csnfromarray(array(1, 0, 0))

    across:CsnArr  = csnreject(a, b)
    across_out:i[] = csntoarray(across)
    prints("rejection = %g %g %g\n", across_out[0], across_out[1], across_out[2])

    ; orthogonal by construction
    check:i        = csndot(across, b)
    prints("dot with b = %g\n", check)

    ; parallel vectors leave nothing behind
    twice:CsnArr   = csnmul(b, 5)
    none:CsnArr    = csnreject(twice, b)
    none_out:i[]   = csntoarray(none)
    prints("parallel  = %g %g %g\n", none_out[0], none_out[1], none_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
