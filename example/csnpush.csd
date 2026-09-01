<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnpush.csd
;
; csnempty reserves the capacity, csnpush fills it. Pushing up to the reserved
; extent costs no allocation; past it the array grows geometrically.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    cap:i[]     = fillarray(4)
    buf:CsnArr  = csnempty(cap)

    csnpush(buf, 10)
    csnpush(buf, 20)
    csnpush(buf, 30)

    n:i         = csnsize(buf)
    buf_out:i[] = csntoarray(buf)
    prints("n = %d, values = %g %g %g\n", n, buf_out[0], buf_out[1], buf_out[2])

    ; past the reservation the array simply grows
    csnpush(buf, 40)
    csnpush(buf, 50)
    grown_n:i   = csnsize(buf)
    grown:i[]   = csntoarray(buf)
    prints("n = %d, last = %g\n", grown_n, grown[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
