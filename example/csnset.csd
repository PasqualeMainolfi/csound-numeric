<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnset.csd
;
; csnset writes in place: no handle comes back, and every consumer downstream
; sees a new generation of the array on its next pass.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]   = fillarray(2, 3)
    mat:CsnArr  = csnzeros(shape)

    cell:i[]    = fillarray(1, 2)
    csnset(mat, cell, 7)
    value:i     = csnget(mat, cell)
    total:i     = csnsum(mat)
    prints("mat[1][2] = %g, sum = %g\n", value, total)

    ; the diagonal of a 3 x 3, written one cell at a time
    square:i[]  = fillarray(3, 3)
    eye:CsnArr  = csnzeros(square)
    n:i = 0
    while n < 3 do
        diag_cell:i[] = fillarray(n, n)
        csnset(eye, diag_cell, 1)
        n += 1
    od
    tr:i        = csntrace(eye)
    prints("trace = %g\n", tr)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
