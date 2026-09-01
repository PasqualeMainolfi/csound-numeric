# csnargmin

## Abstract

Coordinates of the smallest element.

## Description

`csnargmin` reports **where** the smallest element is, not what it is —
[csnmin](csnmin.md) is that.

The answer is always a `matches × ndims` array of full coordinates. Without an
axis there is one match, the global minimum, so the result is a single row of
`ndims` numbers. Given an axis there is one match per line along it, and each row
still carries the complete coordinates of that element, which is what makes the
result usable directly with [csnget](csnget.md).

Real only — ordering has no meaning over the complex field.

## Syntax

```csound
handle:CsnArr = csnargmin(source:CsnArr)
handle:CsnArr = csnargmin(source:CsnArr, axis:i)
handle:CsnArr = csnargmin(source:CsnArr, axis:k)
handle:CsnArr = csnargmin(source:CsnArr, axis:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to scan.
* `axis:i / axis:k` (optional, default `-1`): the axis to search along; `-1` searches the whole array.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: a `matches × ndims` array of coordinates.

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
; csnargmin.csd
;
; The answer is a coordinate, not a value, and it is complete: a row of the
; result feeds straight back into csnget.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(3, 1, 9, 2))
    at:CsnArr     = csnargmin(vec)
    at_out:i[]    = csntoarray(csnflatten(at))
    smallest:i    = csnmin(vec)
    prints("smallest %g at index %g\n", smallest, at_out[0])

    ; on a matrix the row holds both coordinates
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(4, 9, 3, 1, 5, 6)), shape)
    cell:CsnArr   = csnargmin(mat)
    cell_out:i[]  = csntoarray(csnflatten(cell))
    prints("minimum at (%g, %g)\n", cell_out[0], cell_out[1])

    ; and it feeds straight back into csnget
    coord:i[]     = fillarray(cell_out[0], cell_out[1])
    value:i       = csnget(mat, coord)
    prints("value there = %g\n", value)

    ; one match per row, along an axis
    rows:CsnArr   = csnargmin(mat, 1)
    rows_out:i[]  = csntoarray(csnflatten(rows))
    prints("row 0 min at column %g, row 1 at column %g\n", rows_out[1], rows_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnargmax](csnargmax.md)
* [csnmin](csnmin.md)
* [csnargsort](csnargsort.md)
* [csnget](csnget.md)

## Credits

Pasquale Mainolfi, 2026
