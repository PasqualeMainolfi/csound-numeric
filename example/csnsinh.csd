<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsinh.csd
;
; Elementwise, over the whole array. csntanh is the bounded one, which is why
; it is the usual soft-clipper.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(-2, -1, 0, 1, 2))

    out_a:CsnArr  = csnsinh(data)
    out_arr:i[]   = csntoarray(out_a)
    prints("sinh = %.4f %.4f %.4f %.4f %.4f\n", out_arr[0], out_arr[1], out_arr[2], out_arr[3], out_arr[4])

    lo:i          = csnmin(out_a)
    hi:i          = csnmax(out_a)
    prints("range = %.4f .. %.4f\n", lo, hi)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
