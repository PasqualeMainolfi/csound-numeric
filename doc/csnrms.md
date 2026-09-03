# csnrms

## Abstract

Root mean square, over the whole array or along one axis.

## Description

`csnrms` returns `sqrt(mean(x²))`: it squares every element, averages, and takes
the square root. It is the magnitude a signal carries regardless of sign, which
is why it is the usual measure of level for a block of samples.

With no axis argument the whole array is folded and the result is a plain number.
With an axis the fold happens along that axis alone, which disappears from the
result: a `(2, 3)` source reduced along axis 1 gives a `(2,)` array, one value per
row. The divisor is always the extent of the folded axis, so the same source
gives different values along different axes.

Real only — squaring a complex number is not what an amplitude measure wants; for
the magnitude of a complex array use [csnabs](csnabs.md) and reduce that. An
empty array gives `nan`, as [csnmean](csnmean.md) does.

## Syntax

```csound
value:i = csnrms(source:CsnArr)
value:k = csnrms(source:CsnArr, trig:k)
handle:CsnArr = csnrms(source:CsnArr, axis:i)
handle:CsnArr = csnrms(source:CsnArr, axis:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to reduce.
* `axis:i / axis:k`: axis to fold. Without it the array is read flat and the result is a scalar.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `value:i / value:k`: the root mean square of every element, for the scalar form.
* `handle:CsnArr`: handle of one value per position of the surviving axes, for the axis form.

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
; csnrms.csd
;
; The level of a block, then the same matrix reduced along both axes: the
; divisor follows the axis being folded, so the two answers differ.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    block:CsnArr  = csnfromarray(array(3, 4))
    level:i       = csnrms(block)
    prints("level    = %.4f\n", level)

    shape:i[]     = fillarray(2, 3)
    flat:CsnArr   = csnfromarray(array(1, 2, 3, 4, 5, 6))
    mat:CsnArr    = csnreshape(flat, shape)

    ; axis 1: three elements per row, result is 2 values
    rows:CsnArr   = csnrms(mat, 1)
    rows_out:i[]  = csntoarray(rows)
    prints("per row  = %.4f %.4f\n", rows_out[0], rows_out[1])

    ; axis 0: two elements per column, result is 3 values
    cols:CsnArr   = csnrms(mat, 0)
    cols_out:i[]  = csntoarray(cols)
    prints("per col  = %.4f %.4f %.4f\n", cols_out[0], cols_out[1], cols_out[2])
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
* [csnstd](csnstd.md)
* [csnnorm](csnnorm.md)
* [csnabs](csnabs.md)

## Credits

Pasquale Mainolfi, 2026
