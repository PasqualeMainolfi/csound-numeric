<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csntan.csd
;
; The argument is in radians. csndegtorad converts, and csnlinspace is the
; usual way to lay out a phase axis.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    phase:CsnArr  = csnlinspace(0, 6.283185307179586, 5)

    out_a:CsnArr  = csntan(phase)
    out_arr:i[]   = csntoarray(out_a)
    prints("tangent = %.4f %.4f %.4f %.4f %.4f\n", out_arr[0], out_arr[1], out_arr[2], out_arr[3], out_arr[4])

    ; the same thing from degrees
    deg:CsnArr    = csnfromarray(array(0, 90, 180, 270, 360))
    rad:CsnArr    = csndegtorad(deg)
    from_deg:CsnArr = csntan(rad)
    deg_out:i[]   = csntoarray(from_deg)
    prints("again = %.4f %.4f %.4f %.4f %.4f\n", deg_out[0], deg_out[1], deg_out[2], deg_out[3], deg_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
