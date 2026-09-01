<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnatanh.csd
;
; Elementwise, over the whole array. Where the real domain is restricted,
; csnclip is the guard to put in front.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(-0.9, -0.5, 0, 0.5, 0.9))

    out_a:CsnArr  = csnatanh(data)
    out_arr:i[]   = csntoarray(out_a)
    prints("atanh = %.4f %.4f %.4f %.4f %.4f\n", out_arr[0], out_arr[1], out_arr[2], out_arr[3], out_arr[4])

    nan_count:i   = csncntnan(out_a)
    prints("NaN elements = %d\n", nan_count)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
