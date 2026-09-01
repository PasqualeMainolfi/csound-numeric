# csnmedian

## Abstract

Median of the elements.

## Description

`csnmedian` returns the middle value of the elements in sorted order — the mean of
the two middle ones when the count is even.

Unlike [csnmean](csnmean.md) it is barely moved by a few extreme values, which is
what makes it the robust summary of noisy data.

Over the whole array the result is a single number. Given an axis the
reduction runs along that axis instead and the result is an array, with the
reduced axis dropped: a `2×3` matrix reduced on axis 0 gives 3 values, one per
column, and on axis 1 gives 2, one per row.

Real only — ordering has no meaning over the complex field.

## Syntax

```csound
result:i = csnmedian(source:CsnArr)
result:k = csnmedian(source:CsnArr)
result:k = csnmedian(source:CsnArr, trig:k)
handle:CsnArr = csnmedian(source:CsnArr, axis:i)
handle:CsnArr = csnmedian(source:CsnArr, axis:k)
handle:CsnArr = csnmedian(source:CsnArr, axis:k, trig:k)
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
; csnmedian.csd
;
; The robust centre: a few extreme values move the mean and barely move the
; median.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(2, 4, 4, 4, 5, 5, 7, 9))
    mid:i         = csnmedian(data)
    avg:i         = csnmean(data)
    prints("median = %g, mean = %g\n", mid, avg)

    ; one outlier moves the mean and barely moves the median
    spiked:CsnArr = csnfromarray(array(2, 4, 4, 4, 5, 5, 7, 900))
    mid2:i        = csnmedian(spiked)
    avg2:i        = csnmean(spiked)
    prints("with an outlier: median = %g, mean = %g\n", mid2, avg2)

    ; along an axis
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    rows:CsnArr   = csnmedian(mat, 1)
    rows_out:i[]  = csntoarray(rows)
    prints("per row = %g %g\n", rows_out[0], rows_out[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnmean](csnmean.md)
* [csnpercentile](csnpercentile.md)
* [csnquantile](csnquantile.md)
* [csnmovmedian](csnmovmedian.md)

## Credits

Pasquale Mainolfi, 2026
