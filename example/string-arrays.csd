<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; string-arrays.csd
;
; A CsnStr is an array of strings with the layout of a CsnArr. It comes in
; from an S[], is read and written with [], sliced, saved as a NumPy <U array
; and loaded back, and goes out as an S[] again.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    labels:S[]   = fillarray("kick", "snare", "hat", "tom", "crash", "ride")
    grid:CsnStr  = csnfromarray(labels)
    shape:i[]    = fillarray(2, 3)
    csnreshape(grid, shape)                 ; in place: a layout change only
    csnprint(grid)

    grid[1][2]   = "china"                  ; [] stores a copy of the string
    second:S     = grid[0][1]
    kind:i       = csntype(grid)
    count:i      = csnsize(grid)
    prints("grid[0][1] = %s, csntype = %d, csnsize = %d\n", second, kind, count)

    top:CsnStr   = grid[0:0]                ; first row, end included
    top[0][0]    = "808"                    ; the slice owns its strings
    still:S      = grid[0][0]
    changed:S    = top[0][0]
    prints("slice[0][0] = %s, grid[0][0] is still %s\n", changed, still)

    csnsave(grid, "string_arrays_example.npy")   ; np.load gives dtype <U5
    back:CsnStr  = csnload("string_arrays_example.npy")
    rows:S[]     = init(2, 3)               ; an array of S, given its rank
    rows         = csntoarray(back)
    last:S       = rows[1][2]
    prints("loaded back, rows[1][2] = %s\n", last)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
