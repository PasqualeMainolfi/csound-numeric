# csnstd

## Abstract

Standard deviation of the elements.

## Description

`csnstd` returns the population standard deviation: the square root of the mean
squared deviation from the mean, divided by `N` rather than `N - 1`.

Over the whole array the result is a single number. Given an axis the
reduction runs along that axis instead and the result is an array, with the
reduced axis dropped: a `2×3` matrix reduced on axis 0 gives 3 values, one per
column, and on axis 1 gives 2, one per row.

It is the square root of [csnvar](csnvar.md), and undefined over an empty extent.

Both real and complex arrays are accepted; the spread of a complex array is
measured with the magnitudes of the deviations, so the answer is a real number
either way.

## Syntax

```csound
result:i = csnstd(source:CsnArr)
result:k = csnstd(source:CsnArr)
result:k = csnstd(source:CsnArr, trig:k)
handle:CsnArr = csnstd(source:CsnArr, axis:i)
handle:CsnArr = csnstd(source:CsnArr, axis:k)
handle:CsnArr = csnstd(source:CsnArr, axis:k, trig:k)
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
; csnstd.csd
;
; Population standard deviation, divided by N. It is the square root of csnvar,
; in the units of the data itself.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(2, 4, 4, 4, 5, 5, 7, 9))

    sd:i          = csnstd(data)
    variance:i    = csnvar(data)
    avg:i         = csnmean(data)
    prints("mean = %g, std = %g, var = %g\n", avg, sd, variance)

    ; the relation between the two
    check:i       = sd * sd
    prints("std squared = %g\n", check)

    ; along an axis
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 10, 20, 30)), shape)
    rows:CsnArr   = csnstd(mat, 1)
    rows_out:i[]  = csntoarray(rows)
    prints("per row = %.4f %.4f\n", rows_out[0], rows_out[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnvar](csnvar.md)
* [csnmean](csnmean.md)
* [csnmovstd](csnmovstd.md)
* [csnnormalize](csnnormalize.md)

## Credits

Pasquale Mainolfi, 2026
