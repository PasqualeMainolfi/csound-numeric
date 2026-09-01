# csnprod

## Abstract

Product of the elements, over everything or along an axis.

## Description

`csnprod` multiplies the elements of an array together.

Over the whole array the result is a single number, or a `:Complex;` for a complex source. Given an axis the
reduction runs along that axis instead and the result is an array, with the
reduced axis dropped: a `2×3` matrix reduced on axis 0 gives 3 values, one per
column, and on axis 1 gives 2, one per row.

A NaN propagates, and so does a zero: one zero element makes the whole product
zero. The product over an empty array is `1`, the neutral element.

Long products of values far from 1 overflow or underflow quickly; summing
[csnlog](csnlog.md) of the magnitudes is the usual way round that.

Both real and complex arrays are accepted; a complex product comes back as a
`:Complex;`.

## Syntax

```csound
result:i = csnprod(source:CsnArr)
result:k = csnprod(source:CsnArr)
result:k = csnprod(source:CsnArr, trig:k)
result:Complex = csnprod(source:CsnArr)
result:Complex = csnprod(source:CsnArr, trig:k)
handle:CsnArr = csnprod(source:CsnArr, axis:i)
handle:CsnArr = csnprod(source:CsnArr, axis:k)
handle:CsnArr = csnprod(source:CsnArr, axis:k, trig:k)
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
; csnprod.csd
;
; The neutral element here is 1, so an empty array gives 1 rather than 0.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(1, 2, 3, 4))
    whole:i       = csnprod(vec)
    prints("product   = %g\n", whole)

    ; along an axis: one value per column, then one per row
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    cols:CsnArr   = csnprod(mat, 0)
    cols_out:i[]  = csntoarray(cols)
    prints("per column = %g %g %g\n", cols_out[0], cols_out[1], cols_out[2])

    rows:CsnArr   = csnprod(mat, 1)
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

* [csnsum](csnsum.md)
* [csncumprod](csncumprod.md)
* [csnmul](csnmul.md)
* [csnlog](csnlog.md)

## Credits

Pasquale Mainolfi, 2026
