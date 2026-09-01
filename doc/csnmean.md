# csnmean

## Abstract

Arithmetic mean of the elements.

## Description

`csnmean` returns the sum of the elements divided by their count.

Over the whole array the result is a single number, or a `:Complex;` for a complex source. Given an axis the
reduction runs along that axis instead and the result is an array, with the
reduced axis dropped: a `2×3` matrix reduced on axis 0 gives 3 values, one per
column, and on axis 1 gives 2, one per row.

A NaN propagates, so run [csncntnan](csncntnan.md) first over data that may carry
one.

Both real and complex arrays are accepted; the mean of a complex array is the
complex centroid and comes back as a `:Complex;`.

## Syntax

```csound
result:i = csnmean(source:CsnArr)
result:k = csnmean(source:CsnArr)
result:k = csnmean(source:CsnArr, trig:k)
result:Complex = csnmean(source:CsnArr)
result:Complex = csnmean(source:CsnArr, trig:k)
handle:CsnArr = csnmean(source:CsnArr, axis:i)
handle:CsnArr = csnmean(source:CsnArr, axis:k)
handle:CsnArr = csnmean(source:CsnArr, axis:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to reduce.
* `axis:i / axis:k` (optional): the axis to reduce along. Omitted, the whole array is reduced to one number.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `result:i / result:k`: the reduction over the whole array, for a real source.
* `result:Complex`: the reduction over the whole array, for a complex source.
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
; csnmean.csd
;
; The mean over the whole array, then per column and per row. Its usual partners
; are csnstd and csnvar, which measure the spread around it.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(2, 4, 4, 4, 5, 5, 7, 9))
    whole:i       = csnmean(vec)
    prints("mean      = %g\n", whole)

    ; along an axis: one value per column, then one per row
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    cols:CsnArr   = csnmean(mat, 0)
    cols_out:i[]  = csntoarray(cols)
    prints("per column = %g %g %g\n", cols_out[0], cols_out[1], cols_out[2])

    rows:CsnArr   = csnmean(mat, 1)
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

* [csnstd](csnstd.md)
* [csnvar](csnvar.md)
* [csnmedian](csnmedian.md)
* [csnsum](csnsum.md)

## Credits

Pasquale Mainolfi, 2026
