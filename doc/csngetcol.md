# csngetcol

## Abstract

Extract one column from a two-dimensional array.

## Description

`csngetcol` reads a zero-based column from an array with exactly two dimensions
and returns it as a one-dimensional array. A matrix with shape `rows × columns`
therefore produces a vector of `rows` elements. The source element type is
preserved, so both real and complex arrays are supported.

At k-rate, a zero trigger republishes the previous result; a non-zero trigger
reads the requested column again.

## Syntax

```csound
column:CsnArr = csngetcol(handle:CsnArr, index:i)
column:CsnArr = csngetcol(handle:CsnArr, index:k)
column:CsnArr = csngetcol(handle:CsnArr, index:k, trig:k)
```

## Arguments

* `handle:CsnArr`: source array; it must have exactly two dimensions.
* `index:i / index:k`: zero-based column index in `[0, columns)`.
* `trig:k` (optional, default `1`): k-rate trigger. A non-zero value reads the column; zero republishes the previous result.

## Output

* `column:CsnArr`: one-dimensional column, with the same element type as the source.

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
; csngetcol.csd
;
; csngetcol extracts one zero-based column from a two-dimensional array and
; returns it as a one-dimensional array.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]  = fillarray(2, 3)
    matrix:CsnArr = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    column:CsnArr = csngetcol(matrix, 2)

    prints("column 2:\n")
    csnprint(column)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csngetrow](csngetrow.md)
* [csnget](csnget.md)
* [csngetslice](csngetslice.md)
* [csntake](csntake.md)

## Credits

Pasquale Mainolfi, 2026
