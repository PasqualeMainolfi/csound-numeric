<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnhypot.csd
;
; Two component arrays in, one length array out. Computed without the overflow
; a literal sqrt(a*a + b*b) would risk on large values.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    x:CsnArr     = csnfromarray(array(3, 5, 8))
    y:CsnArr     = csnfromarray(array(4, 12, 15))

    len:CsnArr   = csnhypot(x, y)
    len_out:i[]  = csntoarray(len)
    prints("lengths = %g %g %g\n", len_out[0], len_out[1], len_out[2])

    ; a scalar second component: distance from a fixed offset
    off:CsnArr   = csnhypot(x, 4)
    off_out:i[]  = csntoarray(off)
    prints("with 4  = %g %.4f %.4f\n", off_out[0], off_out[1], off_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
