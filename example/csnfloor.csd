<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnfloor.csd
;
; Floor goes down, always: -1.2 becomes -2. That is what separates it from
; csnround, which goes to the nearest.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr  = csnfromarray(array(-1.2, -0.5, 0.5, 1.7))

    down:CsnArr  = csnfloor(data)
    down_out:i[] = csntoarray(down)
    prints("floor = %g %g %g %g\n", down_out[0], down_out[1], down_out[2], down_out[3])

    ; the three roundings side by side
    up:CsnArr    = csnceil(data)
    near:CsnArr  = csnround(data)
    up_out:i[]   = csntoarray(up)
    near_out:i[] = csntoarray(near)
    prints("ceil  = %g %g %g %g\n", up_out[0], up_out[1], up_out[2], up_out[3])
    prints("round = %g %g %g %g   (halves go to the even neighbour)\n", near_out[0], near_out[1], near_out[2], near_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
