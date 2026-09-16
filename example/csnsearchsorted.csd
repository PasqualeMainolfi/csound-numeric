<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsearchsorted.csd
;
; Find insertion points on either side of a duplicate run. The source is
; already sorted; searchsorted never rearranges or validates it.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    sorted:CsnArr    = csnfromarray(array(1, 3, 3, 5))
    wanted:CsnArr    = csnfromarray(array(0, 3, 4, 6))

    left:CsnArr      = csnsearchsorted(sorted, wanted)
    right:CsnArr     = csnsearchsorted(sorted, wanted, 1)
    left_out:i[]     = csntoarray(left)
    right_out:i[]    = csntoarray(right)
    prints("left  = %g %g %g %g\n", left_out[0], left_out[1], left_out[2], left_out[3])
    prints("right = %g %g %g %g\n", right_out[0], right_out[1], right_out[2], right_out[3])

    left_three:i     = csnsearchsorted(sorted, 3)
    right_three:i    = csnsearchsorted(sorted, 3, 1)
    prints("3 spans insertion indices %g through %g\n", left_three, right_three)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
