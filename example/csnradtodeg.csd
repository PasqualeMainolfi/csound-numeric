<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnradtodeg.csd
;
; The trigonometric opcodes answer in radians. csnradtodeg is what turns that
; answer into something readable.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    slope:CsnArr = csnfromarray(array(0, 1, -1, 1000))

    ang:CsnArr   = csnatan(slope)
    deg:CsnArr   = csnradtodeg(ang)
    deg_out:i[]  = csntoarray(deg)
    prints("atan in degrees = %.3f %.3f %.3f %.3f\n", deg_out[0], deg_out[1], deg_out[2], deg_out[3])

    ; the round trip
    back:CsnArr  = csndegtorad(deg)
    back_out:i[] = csntoarray(back)
    ang_out:i[]  = csntoarray(ang)
    prints("round trip: %.6f vs %.6f\n", back_out[1], ang_out[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
