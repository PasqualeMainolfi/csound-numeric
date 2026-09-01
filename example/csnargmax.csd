<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnargmax.csd
;
; Finding the dominant bin of a magnitude curve: csnargmax gives the index,
; csnmax gives the height.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    mags:CsnArr   = csnfromarray(array(0.1, 0.3, 0.9, 0.4, 0.2))

    at:CsnArr     = csnargmax(mags)
    at_out:i[]    = csntoarray(csnflatten(at))
    peak:i        = csnmax(mags)
    prints("peak %.1f at bin %g\n", peak, at_out[0])

    ; turn the bin into a frequency
    bin_width:i   = 44100 / 1024
    freq:i        = at_out[0] * bin_width
    prints("that bin is %.1f Hz wide bins in, %.1f Hz\n", bin_width, freq)

    ; per row of a matrix
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 9, 3, 4, 5, 6)), shape)
    rows:CsnArr   = csnargmax(mat, 1)
    rows_out:i[]  = csntoarray(csnflatten(rows))
    prints("row 0 max at column %g, row 1 at column %g\n", rows_out[1], rows_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
