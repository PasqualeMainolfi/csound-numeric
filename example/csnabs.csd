<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnabs.csd
;
; csnabs drops the sign of a real array. Its usual companion is csnsum, which
; turns the pair into a sum of magnitudes.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr = csnfromarray(array(-3, 1, -4, 1, -5))

    mag:CsnArr  = csnabs(data)
    mag_out:i[] = csntoarray(mag)
    prints("abs = %g %g %g %g %g\n", mag_out[0], mag_out[1], mag_out[2], mag_out[3], mag_out[4])

    ; sum of magnitudes, the L1 norm read the long way
    total:i     = csnsum(mag)
    l1:i        = csnnorm(data, 1)
    prints("sum of abs = %g, csnnorm order 1 = %g\n", total, l1)

    ; Complex input still produces one real magnitude per complex element.
    complex_data:CsnArr = csntocomplex(csnfromarray(array(3, -4, 0, 5)))
    complex_mag:CsnArr = csnabs(complex_data)
    complex_out:i[] = csntoarray(complex_mag)
    prints("complex magnitudes = %g %g %g %g\n", complex_out[0], complex_out[1], complex_out[2], complex_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
