<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnceil.csd
;
; Ceil goes up, always. Combined with a division it is the way to count how many
; blocks of a given size a run of samples needs.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr  = csnfromarray(array(-1.2, -0.5, 0.5, 1.7))

    up:CsnArr    = csnceil(data)
    up_out:i[]   = csntoarray(up)
    prints("ceil = %g %g %g %g\n", up_out[0], up_out[1], up_out[2], up_out[3])

    ; how many blocks of 512 do these lengths need
    lens:CsnArr  = csnfromarray(array(100, 512, 513, 2000))
    blocks:CsnArr = csnceil(csndiv(lens, 512))
    blocks_out:i[] = csntoarray(blocks)
    prints("blocks = %g %g %g %g\n", blocks_out[0], blocks_out[1], blocks_out[2], blocks_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
