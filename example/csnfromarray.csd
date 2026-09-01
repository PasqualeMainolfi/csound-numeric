<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnfromarray.csd
;
; csnfromarray is the way into the suite. From there on everything travels as a
; handle, and csntoarray is the way back out.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(4, 1, 3, 2))
    sorted:CsnArr  = csnsort(data)
    sorted_out:i[] = csntoarray(sorted)
    prints("sorted = %g %g %g %g\n", sorted_out[0], sorted_out[1], sorted_out[2], sorted_out[3])

    ; rank and extents survive the trip
    src:i[][]   = init(2, 3)
    src[0][0]   = 1
    src[1][2]   = 6
    mat:CsnArr  = csnfromarray(src)
    dims:i      = csndims(mat)
    size:i      = csnsize(mat)
    prints("dims = %d, size = %d\n", dims, size)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
