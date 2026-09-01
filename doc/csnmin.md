# csnmin

## Abstract

Smallest element.

## Description

`csnmin` returns the smallest element of an array.

Over the whole array the result is a single number. Given an axis the
reduction runs along that axis instead and the result is an array, with the
reduced axis dropped: a `2×3` matrix reduced on axis 0 gives 3 values, one per
column, and on axis 1 gives 2, one per row.

A NaN propagates rather than being skipped: IEEE comparisons against a NaN are
all false, which would quietly drop it, so it is forced through instead and the
answer is a NaN.

There is no smallest element of nothing, so the reduction is undefined over an
empty extent and refused. Real only — ordering has no meaning over the complex
field.

## Syntax

```csound
result:i = csnmin(source:CsnArr)
result:k = csnmin(source:CsnArr)
result:k = csnmin(source:CsnArr, trig:k)
handle:CsnArr = csnmin(source:CsnArr, axis:i)
handle:CsnArr = csnmin(source:CsnArr, axis:k)
handle:CsnArr = csnmin(source:CsnArr, axis:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to reduce.
* `axis:i / axis:k` (optional): the axis to reduce along. Omitted, the whole array is reduced to one number.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `result:i / result:k`: the reduction over the whole array.
* `handle:CsnArr`: one value per line along `axis`, with that axis dropped.

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
; csnmin.csd
;
; Real only: ordering has no meaning over the complex field. A NaN propagates
; rather than being skipped.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(3, 1, 4, 1, 5))
    whole:i       = csnmin(vec)
    prints("minimum   = %g\n", whole)

    ; along an axis: one value per column, then one per row
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    cols:CsnArr   = csnmin(mat, 0)
    cols_out:i[]  = csntoarray(cols)
    prints("per column = %g %g %g\n", cols_out[0], cols_out[1], cols_out[2])

    rows:CsnArr   = csnmin(mat, 1)
    rows_out:i[]  = csntoarray(rows)
    prints("per row    = %g %g\n", rows_out[0], rows_out[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnmax](csnmax.md)
* [csnargmin](csnargmin.md)
* [csnmedian](csnmedian.md)
* [csnclip](csnclip.md)

## Credits

Pasquale Mainolfi, 2026
