<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnacos.csd
;
; The result is in radians. csnradtodeg converts it back for reading.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(-1, -0.5, 0, 0.5, 1))
    safe:CsnArr   = csnclip(data, -1, 1)
    ang:CsnArr    = csnacos(safe)
    ang_out:i[]   = csntoarray(ang)
    prints("acos = %.4f %.4f %.4f %.4f %.4f\n", ang_out[0], ang_out[1], ang_out[2], ang_out[3], ang_out[4])

    ; radians out, degrees for reading
    deg:CsnArr    = csnradtodeg(ang)
    deg_out:i[]   = csntoarray(deg)
    prints("degrees = %.2f %.2f %.2f %.2f %.2f\n", deg_out[0], deg_out[1], deg_out[2], deg_out[3], deg_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
