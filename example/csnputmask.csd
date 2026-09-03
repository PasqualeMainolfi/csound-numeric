<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; The mask array is spent in place: its 1/0 pattern is replaced by the values
; it selects, with no second handle allocated.

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    sig:CsnArr    = csnfromarray(array(0.2, 0.9, 0.4, 1.6))
    gate:CsnArr   = csngt(sig, 0.5)
    loud:CsnArr   = csnfromarray(array(1, 1, 1, 1))

    csnputmask gate, loud, -1

    gate_out:i[]  = csntoarray(gate)
    prints("mask spent = %g %g %g %g\n", gate_out[0], gate_out[1], gate_out[2], gate_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
