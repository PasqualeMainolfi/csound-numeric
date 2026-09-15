# csnindexof

## Abstract

Return the coordinates of the first element equal to a scalar.

## Description

`csnindexof` scans a real array in row-major order and returns the full
coordinates of the first element exactly equal to `value`. A vector therefore
returns a one-element coordinate array; a matrix returns `(row, column)`; an
N-dimensional source returns one coordinate for each of its N dimensions.

Only the first occurrence is returned. Use [csnargwhere](csnargwhere.md) when
all occurrences, or several wanted values, are needed. When no element matches,
the result is an empty 1-D array rather than an error.

Real only. `value` must be finite, and equality is exact.

## Syntax

```csound
handle:CsnArr = csnindexof(source:CsnArr, value:i)
handle:CsnArr = csnindexof(source:CsnArr, value:k)
handle:CsnArr = csnindexof(source:CsnArr, value:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to scan.
* `value:i / value:k`: the finite scalar value to find.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: a 1-D coordinate array of length `csndims(source)`, or an empty array when `value` is absent.

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
; csnindexof.csd
;
; Find the first occurrence of one scalar. The result is one coordinate per
; source dimension; a missing value produces an empty array.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr     = csnfromarray(array(1, 5, 3, 5, 2))
    needle:i       = 5
    first:CsnArr   = csnindexof(vec, needle)
    first_out:i[]  = csntoarray(first)
    prints("first 5 in the vector is at index %g\n", first_out[0])

    ; A matrix result contains (row, column).
    shape:i[]      = fillarray(2, 3)
    mat:CsnArr     = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    cell:CsnArr    = csnindexof(mat, needle)
    cell_out:i[]   = csntoarray(cell)
    prints("first 5 in the matrix is at (%g, %g)\n", cell_out[0], cell_out[1])

    ; Nothing found is represented by an empty coordinate array.
    missing:i      = 99
    none:CsnArr    = csnindexof(mat, missing)
    prints("no match: size = %d\n", csnsize(none))
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnargwhere](csnargwhere.md)
* [csnargnonzero](csnargnonzero.md)
* [csncnteq](csncnteq.md)
* [csnget](csnget.md)

## Credits

Pasquale Mainolfi, 2026
