<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnmedfilt.csd
;
; The N-D median filter: a box centred on every element, zero outside the array.
; One size filters every axis alike; one size per axis filters only the axes you
; give a size greater than one, which is how a column or a row filter is asked
; for.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]       = fillarray(3, 4)
    flat:CsnArr     = csnfromarray(array(1, 9, 2, 3, 4, 1, 8, 2, 3, 5, 1, 7))
    source:CsnArr   = csnreshape(flat, shape)
    csnprint source

    ; a 3x3 box: on a matrix this small most windows are half padding, and the
    ; border comes back close to zero
    square:CsnArr   = csnmedfilt(source, 3)
    csnprint square

    ; one size per axis: 3 down the columns, 1 across, so the rows are untouched
    kcol:i[]        = fillarray(3, 1)
    columns:CsnArr  = csnmedfilt(source, kcol)
    csnprint columns

    ; and the transpose of that, a filter along the rows only
    krow:i[]        = fillarray(1, 3)
    rows:CsnArr     = csnmedfilt(source, krow)
    csnprint rows

    ; in place, no new handle: the source is rewritten
    copy:CsnArr     = csncopy(source)
    csnmedfilt(copy, 3)
    csnprint copy
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
