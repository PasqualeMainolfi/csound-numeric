<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csntoftable.csd
;
; csntoftable hands an array back to the table world. resize = 1 sizes the
; table to the array, so a computed window does not need a matching f-statement.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; a Kaiser window computed here, then published as table 2
    win:CsnArr = csnkaiser(16, 8)
    csntoftable(win, 2, 1)

    len:i      = ftlen(2)
    centre:i   = table(8, 2)
    edge:i     = table(0, 2)
    prints("table 2: len = %d, centre = %.4f, edge = %.4f\n", len, centre, edge)

    ; writing into a table that already exists, without resizing
    ramp:CsnArr = csnlinspace(0, 1, 8)
    csntoftable(ramp, 3)
    first:i    = table(0, 3)
    last:i     = table(7, 3)
    prints("table 3: first = %g, eighth = %g\n", first, last)
    turnoff
endin

</CsInstruments>
<CsScore>
f 3 0 8 2 0
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
