<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnacosh.csd
;
; Elementwise, over the whole array. Where the real domain is restricted,
; csnclip is the guard to put in front.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(1, 1.5, 2, 5, 10))

    out_a:CsnArr  = csnacosh(data)
    out_arr:i[]   = csntoarray(out_a)
    prints("acosh = %.4f %.4f %.4f %.4f %.4f\n", out_arr[0], out_arr[1], out_arr[2], out_arr[3], out_arr[4])

    nan_count:i   = csncntnan(out_a)
    prints("NaN elements = %d\n", nan_count)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
