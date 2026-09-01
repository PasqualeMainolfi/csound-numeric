<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnresample.csd
;
; The endpoints are kept and the positions between them are interpolated, so a
; short envelope becomes a long one without moving where it starts and ends.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr  = csnfromarray(array(0, 10, 20, 30))

    up:CsnArr    = csnresample(data, 7, 0, 1)
    up_out:i[]   = csntoarray(up)
    up_n:i       = csnsize(up)
    prints("to 7, linear : n = %d : %g %g %g %g %g %g %g\n", up_n, up_out[0], up_out[1], up_out[2], up_out[3], up_out[4], up_out[5], up_out[6])

    ; down, and with a different mode
    down:CsnArr  = csnresample(data, 3, 0, 1)
    down_out:i[] = csntoarray(down)
    prints("to 3, linear : %g %g %g\n", down_out[0], down_out[1], down_out[2])

    held:CsnArr  = csnresample(data, 7, 2, 1)
    held_out:i[] = csntoarray(held)
    prints("to 7, hold   : %g %g %g %g %g %g %g\n", held_out[0], held_out[1], held_out[2], held_out[3], held_out[4], held_out[5], held_out[6])

    ; a short envelope stretched to fill a table
    env:CsnArr   = csnfromarray(array(0, 1, 0.6, 0))
    long_env:CsnArr = csnresample(env, 16, 4, 1)
    long_n:i     = csnsize(long_env)
    peak:i       = csnmax(long_env)
    prints("envelope: n = %d, peak = %g\n", long_n, peak)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
