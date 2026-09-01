<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnround.csd
;
; Nearest integer, halves to the even neighbour as in NumPy. Scale up and back
; down to round to a resolution other than 1.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(-1.2, 0.5, 1.5, 2.5, 1.7))

    near:CsnArr    = csnround(data)
    near_out:i[]   = csntoarray(near)
    prints("round = %g %g %g %g %g\n", near_out[0], near_out[1], near_out[2], near_out[3], near_out[4])

    ; quantise a set of frequency ratios to two decimals
    ratios:CsnArr  = csnfromarray(array(1.4983, 1.2599, 1.0595))
    scaled:CsnArr  = csnmul(ratios, 100)
    q:CsnArr       = csndiv(csnround(scaled), 100)
    q_out:i[]      = csntoarray(q)
    prints("two decimals = %g %g %g\n", q_out[0], q_out[1], q_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
