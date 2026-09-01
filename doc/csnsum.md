# csnsum

## Abstract

Sum of the elements, over everything or along an axis.

## Description

`csnsum` adds the elements of an array together.

Over the whole array the result is a single number, or a `:Complex;` for a complex source. Given an axis the
reduction runs along that axis instead and the result is an array, with the
reduced axis dropped: a `2×3` matrix reduced on axis 0 gives 3 values, one per
column, and on axis 1 gives 2, one per row.

A NaN propagates: a sum touching one is a NaN. The sum over an empty array is `0`,
the neutral element, so an accumulator needs no special first case.

Both real and complex arrays are accepted; a complex sum comes back as a
`:Complex;`.

## Syntax

```csound
result:i = csnsum(source:CsnArr)
result:k = csnsum(source:CsnArr)
result:k = csnsum(source:CsnArr, trig:k)
result:Complex = csnsum(source:CsnArr)
result:Complex = csnsum(source:CsnArr, trig:k)
handle:CsnArr = csnsum(source:CsnArr, axis:i)
handle:CsnArr = csnsum(source:CsnArr, axis:k)
handle:CsnArr = csnsum(source:CsnArr, axis:k, trig:k)
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
; csnsum.csd
;
; Whole-array first, then per column and per row. The sum over an empty array is
; 0, so an accumulator needs no special first case.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(1, 2, 3, 4))
    whole:i       = csnsum(vec)
    prints("sum       = %g\n", whole)

    ; along an axis: one value per column, then one per row
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    cols:CsnArr   = csnsum(mat, 0)
    cols_out:i[]  = csntoarray(cols)
    prints("per column = %g %g %g\n", cols_out[0], cols_out[1], cols_out[2])

    rows:CsnArr   = csnsum(mat, 1)
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

* [csnprod](csnprod.md)
* [csnmean](csnmean.md)
* [csncumsum](csncumsum.md)
* [csnsub](csnsub.md)

## Credits

Pasquale Mainolfi, 2026
