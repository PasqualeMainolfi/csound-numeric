# csnpercentile

## Abstract

Percentile of the elements, from 0 to 100.

## Description

`csnpercentile` returns the value below which a given percentage of the elements
fall. `50` is the median, `0` the minimum and `100` the maximum.

When the requested position falls between two elements the result is
interpolated linearly between them, as NumPy does by default, so a percentile is
not necessarily an element of the array.

[csnquantile](csnquantile.md) is the same reduction with the fraction stated from
0 to 1 instead.

Over the whole array the result is a single number. Given an axis the reduction
runs along that axis and the result is an array, with the reduced axis dropped.

Real only — ordering has no meaning over the complex field.

## Syntax

```csound
value:i = csnpercentile(source:CsnArr, percent:i)
value:k = csnpercentile(source:CsnArr, percent:k)
value:k = csnpercentile(source:CsnArr, percent:k, trig:k)
handle:CsnArr = csnpercentile(source:CsnArr, percent:i, axis:i)
handle:CsnArr = csnpercentile(source:CsnArr, percent:k, axis:k)
handle:CsnArr = csnpercentile(source:CsnArr, percent:k, axis:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to reduce.
* `percent:i / percent:k`: the percentile, from 0 to 100.
* `axis:i / axis:k` (optional): the axis to reduce along. Omitted, the whole array is reduced to one number.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `value:i / value:k`: the percentile over the whole array.
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
; csnpercentile.csd
;
; 50 is the median, 0 the minimum, 100 the maximum. Positions between two
; elements are interpolated, so the answer need not be an element of the array.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr  = csnfromarray(array(2, 4, 4, 4, 5, 5, 7, 9))

    p0:i         = csnpercentile(data, 0)
    p25:i        = csnpercentile(data, 25)
    p50:i        = csnpercentile(data, 50)
    p75:i        = csnpercentile(data, 75)
    p100:i       = csnpercentile(data, 100)
    prints("0=%g 25=%g 50=%g 75=%g 100=%g\n", p0, p25, p50, p75, p100)

    ; the ends agree with csnmin and csnmax, the middle with csnmedian
    lo:i         = csnmin(data)
    hi:i         = csnmax(data)
    mid:i        = csnmedian(data)
    prints("min=%g max=%g median=%g\n", lo, hi, mid)

    ; the interquartile range, a robust measure of spread
    iqr:i        = p75 - p25
    prints("interquartile range = %g\n", iqr)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnquantile](csnquantile.md)
* [csnmedian](csnmedian.md)
* [csnmin](csnmin.md)
* [csnmax](csnmax.md)

## Credits

Pasquale Mainolfi, 2026
