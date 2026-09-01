<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnmul.csd
;
; Elementwise, not matrix: csnmul scales, masks and windows. csnmatmul is the
; linear-algebra product.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr       = csnfromarray(array(1, 2, 3, 4))
    b:CsnArr       = csnfromarray(array(10, 100, 1000, 10000))

    prod:CsnArr    = csnmul(a, b)
    prod_out:i[]   = csntoarray(prod)
    prints("elementwise = %g %g %g %g\n", prod_out[0], prod_out[1], prod_out[2], prod_out[3])

    gain:CsnArr    = csnmul(a, 0.5)
    gain_out:i[]   = csntoarray(gain)
    prints("scaled      = %g %g %g %g\n", gain_out[0], gain_out[1], gain_out[2], gain_out[3])

    ; windowing a buffer is one multiplication
    buf:CsnArr     = csnones(fillarray(8))
    win:CsnArr     = csnhanning(8)
    shaped:CsnArr  = csnmul(buf, win)
    shaped_out:i[] = csntoarray(shaped)
    prints("windowed    = %.3f %.3f %.3f %.3f\n", shaped_out[0], shaped_out[1], shaped_out[2], shaped_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
