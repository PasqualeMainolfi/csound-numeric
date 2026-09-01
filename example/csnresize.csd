<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnresize.csd
;
; csnresize does not require the element counts to match: it keeps what fits,
; zero-fills what it grows, and drops what no longer has room.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr     = csnfromarray(array(1, 2, 3, 4))

    bigger:i[]     = fillarray(6)
    grown:CsnArr   = csnresize(vec, bigger)
    grown_out:i[]  = csntoarray(grown)
    prints("grown  = %g %g %g %g %g %g\n", grown_out[0], grown_out[1], grown_out[2], grown_out[3], grown_out[4], grown_out[5])

    smaller:i[]    = fillarray(2)
    shrunk:CsnArr  = csnresize(vec, smaller)
    shrunk_out:i[] = csntoarray(shrunk)
    prints("shrunk = %g %g\n", shrunk_out[0], shrunk_out[1])

    ; and it can change the rank at the same time
    square:i[]     = fillarray(3, 3)
    mat:CsnArr     = csnresize(vec, square)
    mat_shape:i[]  = csnshape(mat)
    mat_size:i     = csnsize(mat)
    prints("as %g x %g, size = %d\n", mat_shape[0], mat_shape[1], mat_size)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
