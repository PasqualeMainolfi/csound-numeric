<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnselect.csd
;
; A mask keeps the elements it marks and drops the rest. The result is always
; 1-D and as long as the number of marks, whatever the shape of the source.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(10, 20, 30, 40))
    mask:CsnArr   = csnfromarray(array(0, 1, 1, 0))

    kept:CsnArr   = csnselect(vec, mask)
    kept_out:i[]  = csntoarray(kept)
    prints("kept   = %g %g\n", kept_out[0], kept_out[1])

    ; the usual source of a mask is a comparison
    sig:CsnArr    = csnfromarray(array(0.2, 0.9, 0.4, 1.6))
    loud:CsnArr   = csngt(sig, 0.5)
    peaks:CsnArr  = csnselect(sig, loud)
    peaks_out:i[] = csntoarray(peaks)
    n:i           = csnsize(peaks)
    prints("above  = %d values: %g %g\n", n, peaks_out[0], peaks_out[1])

    ; a 2-D source needs a 2-D mask of the same shape; the result is flat
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    corners:CsnArr = csnreshape(csnfromarray(array(1, 0, 0, 0, 0, 1)), shape)
    picked:CsnArr = csnselect(mat, corners)
    picked_out:i[] = csntoarray(picked)
    prints("corners = %g %g\n", picked_out[0], picked_out[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
