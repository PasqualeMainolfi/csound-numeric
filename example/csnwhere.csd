<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; A comparison builds the mask, csnwhere spends it: every element above the
; threshold keeps its value, the others are floored to a constant.

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    sig:CsnArr    = csnfromarray(array(0.2, 0.9, 0.4, 1.6))
    gate:CsnArr   = csngt(sig, 0.5)

    kept:CsnArr   = csnwhere(gate, sig, 0)
    kept_out:i[]  = csntoarray(kept)
    prints("gated  = %g %g %g %g\n", kept_out[0], kept_out[1], kept_out[2], kept_out[3])

    quiet:CsnArr  = csnmul(sig, 0.1)
    mixed:CsnArr  = csnwhere(gate, sig, quiet)
    mixed_out:i[] = csntoarray(mixed)
    prints("mixed  = %g %g %g %g\n", mixed_out[0], mixed_out[1], mixed_out[2], mixed_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
