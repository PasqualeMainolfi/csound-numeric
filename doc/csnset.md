# csnset

## Abstract

Write one element of an array by its coordinates.

## Description

`csnset` writes a single element, addressed by a Csound array of coordinates —
one per dimension, in order, exactly as [csnget](csnget.md) reads it. The array
is modified in place and nothing is published.

A complex array takes a `:Complex;` value. The coordinates and the value may be
at different rates, so an i-time cell can be written with a k-rate value and the
other way round.

Writing bumps the array's data version, so every k-rate consumer downstream sees
a new generation on its next pass and recomputes.

## Syntax

```csound
csnset(handle:CsnArr, cell:i[], value:i)
csnset(handle:CsnArr, cell:i[], value:k)
csnset(handle:CsnArr, cell:i[], value:Complex)
csnset(handle:CsnArr, cell:k[], value:i)
csnset(handle:CsnArr, cell:k[], value:k)
csnset(handle:CsnArr, cell:k[], value:Complex)
```

## Arguments

* `handle:CsnArr`: the array to write into.
* `cell:i[] / cell:k[]`: the coordinates, one per dimension, each within its extent.
* `value:i / value:k / value:Complex`: the value to store.

## Output

None. The array is modified.

## Execution Time

* Init
* Performance (k-rate)

## Examples

```csound
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
```

## See also

* [csnget](csnget.md)
* [csnsetslice](csnsetslice.md)
* [csninsert](csninsert.md)

## Credits

Pasquale Mainolfi, 2026
