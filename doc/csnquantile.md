# csnquantile

## Abstract

Quantile of the elements, from 0 to 1.

## Description

`csnquantile` returns the value below which a given fraction of the elements
fall. `0.5` is the median, `0` the minimum and `1` the maximum.

It is [csnpercentile](csnpercentile.md) with the fraction stated from 0 to 1
instead of 0 to 100, and it interpolates the same way: when the requested
position falls between two elements the result is interpolated linearly between
them, so a quantile is not necessarily an element of the array.

Over the whole array the result is a single number. Given an axis the reduction
runs along that axis and the result is an array, with the reduced axis dropped.

Real only — ordering has no meaning over the complex field.

## Syntax

```csound
value:i = csnquantile(source:CsnArr, q:i)
value:k = csnquantile(source:CsnArr, q:k)
value:k = csnquantile(source:CsnArr, q:k, trig:k)
handle:CsnArr = csnquantile(source:CsnArr, q:i, axis:i)
handle:CsnArr = csnquantile(source:CsnArr, q:k, axis:k)
handle:CsnArr = csnquantile(source:CsnArr, q:k, axis:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to reduce.
* `q:i / q:k`: the quantile, from 0 to 1.
* `axis:i / axis:k` (optional): the axis to reduce along. Omitted, the whole array is reduced to one number.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `value:i / value:k`: the quantile over the whole array.
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
; csnquantile.csd
;
; The same reduction as csnpercentile with the fraction stated from 0 to 1.
; Along an axis it answers per row or per column.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr  = csnfromarray(array(2, 4, 4, 4, 5, 5, 7, 9))

    q25:i        = csnquantile(data, 0.25)
    q50:i        = csnquantile(data, 0.5)
    q90:i        = csnquantile(data, 0.9)
    prints("q0.25=%g q0.5=%g q0.9=%.2f\n", q25, q50, q90)

    ; the same thing said in percent
    p25:i        = csnpercentile(data, 25)
    prints("csnpercentile(25) = %g\n", p25)

    ; per row of a matrix
    shape:i[]    = fillarray(2, 3)
    mat:CsnArr   = csnreshape(csnfromarray(array(1, 2, 3, 10, 20, 30)), shape)
    rows:CsnArr  = csnquantile(mat, 0.5, 1)
    rows_out:i[] = csntoarray(rows)
    prints("median per row = %g %g\n", rows_out[0], rows_out[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnpercentile](csnpercentile.md)
* [csnmedian](csnmedian.md)
* [csnsort](csnsort.md)

## Credits

Pasquale Mainolfi, 2026
