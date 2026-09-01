<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnload.csd
;
; csnload restores shape and element type from the file header. At k-rate the
; trigger is the whole contract, and the handle is empty until it first fires.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]  = fillarray(3, 2)
    src:CsnArr = csnreshape(csnfromarray(array(10, 20, 30, 40, 50, 60)), shape)
    csnsave(src, "csnload_example.csn")
    prints("written\n")
    turnoff
endin

instr 2
    back:CsnArr     = csnload("csnload_example.csn")
    back_shape:i[]  = csnshape(back)
    back_out:i[]    = csntoarray(csnflatten(back))
    prints("shape = %g x %g, values = %g %g %g %g %g %g\n", back_shape[0], back_shape[1], back_out[0], back_out[1], back_out[2], back_out[3], back_out[4], back_out[5])
    turnoff
endin

instr 3
    ; k-rate: empty until the trigger fires
    elapsed:k   = timeinsts()
    trig:k      = (elapsed > 0.02 ? 1 : 0)
    live:CsnArr = csnload("csnload_example.csn", trig)
    n:k         = csnsize(live)
    printf("size after trigger = %d\n", trig, n)
endin

</CsInstruments>
<CsScore>
i 1 0   0.1
i 2 0.2 0.1
i 3 0.4 0.1
</CsScore>
</CsoundSynthesizer>
