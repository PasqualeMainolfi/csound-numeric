# csnvar

## Abstract

Variance of the elements.

## Description

`csnvar` returns the population variance: the mean squared deviation from the
mean, divided by `N` rather than `N - 1`.

Over the whole array the result is a single number. Given an axis the
reduction runs along that axis instead and the result is an array, with the
reduced axis dropped: a `2×3` matrix reduced on axis 0 gives 3 values, one per
column, and on axis 1 gives 2, one per row.

It is the square of [csnstd](csnstd.md) and carries the square of the data's
units, which is why the standard deviation is usually the one reported.
Undefined over an empty extent.

Both real and complex arrays are accepted; the spread of a complex array is
measured with the magnitudes of the deviations, so the answer is a real number
either way.

## Syntax

```csound
result:i = csnvar(source:CsnArr)
result:k = csnvar(source:CsnArr)
result:k = csnvar(source:CsnArr, trig:k)
handle:CsnArr = csnvar(source:CsnArr, axis:i)
handle:CsnArr = csnvar(source:CsnArr, axis:k)
handle:CsnArr = csnvar(source:CsnArr, axis:k, trig:k)
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
; csnvar.csd
;
; Population variance, divided by N. The square of csnstd, in the square of the
; data's units.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(2, 4, 4, 4, 5, 5, 7, 9))

    variance:i    = csnvar(data)
    sd:i          = csnstd(data)
    prints("var = %g, std = %g\n", variance, sd)

    ; a constant array has no spread at all
    flat:CsnArr   = csnfull(fillarray(5), 3)
    flat_var:i    = csnvar(flat)
    prints("variance of a constant array = %g\n", flat_var)

    ; along an axis
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 10, 20, 30)), shape)
    cols:CsnArr   = csnvar(mat, 0)
    cols_out:i[]  = csntoarray(cols)
    prints("per column = %.2f %.2f %.2f\n", cols_out[0], cols_out[1], cols_out[2])
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
* [csnmean](csnmean.md)
* [csnmovvar](csnmovvar.md)

## Credits

Pasquale Mainolfi, 2026
