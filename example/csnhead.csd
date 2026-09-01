<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnhead.csd
;
; csnhead keeps the front of a vector. Combined with csnroll it reads a sliding
; window off a ring without moving the ring.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr     = csnfromarray(array(10, 20, 30, 40, 50, 60))

    head:CsnArr    = csnhead(vec, 3)
    head_out:i[]   = csntoarray(head)
    n:i            = csnsize(head)
    prints("first %d = %g %g %g\n", n, head_out[0], head_out[1], head_out[2])

    ; roll first, then read the head: a window that slides without copying
    rolled:CsnArr  = csnroll(vec, -2)
    window:CsnArr  = csnhead(rolled, 3)
    window_out:i[] = csntoarray(window)
    prints("window  = %g %g %g\n", window_out[0], window_out[1], window_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
