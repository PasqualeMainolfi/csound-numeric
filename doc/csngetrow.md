# csngetrow

## Abstract

Extract one row from a two-dimensional array.

## Description

`csngetrow` reads a zero-based row from an array with exactly two dimensions and
returns it as a one-dimensional array. A matrix with shape `rows × columns`
therefore produces a vector of `columns` elements. The source element type is
preserved, so both real and complex arrays are supported.

At k-rate, a zero trigger republishes the previous result; a non-zero trigger
reads the requested row again.

## Syntax

```csound
row:CsnArr = csngetrow(handle:CsnArr, index:i)
row:CsnArr = csngetrow(handle:CsnArr, index:k)
row:CsnArr = csngetrow(handle:CsnArr, index:k, trig:k)
```

## Arguments

* `handle:CsnArr`: source array; it must have exactly two dimensions.
* `index:i / index:k`: zero-based row index in `[0, rows)`.
* `trig:k` (optional, default `1`): k-rate trigger. A non-zero value reads the row; zero republishes the previous result.

## Output

* `row:CsnArr`: one-dimensional row, with the same element type as the source.

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
; csngetrow.csd
;
; csngetrow extracts one zero-based row from a two-dimensional array and
; returns it as a one-dimensional array.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]  = fillarray(2, 3)
    matrix:CsnArr = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    row:CsnArr = csngetrow(matrix, 1)

    prints("row 1:\n")
    csnprint(row)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csngetcol](csngetcol.md)
* [csnget](csnget.md)
* [csngetslice](csngetslice.md)
* [csntake](csntake.md)

## Credits

Pasquale Mainolfi, 2026
