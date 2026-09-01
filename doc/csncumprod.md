# csncumprod

## Abstract

Cumulative product of the elements.

## Description

`csncumprod` returns a running product: element `n` of the result is the product
of elements `0 … n` of the source. The shape is unchanged, so `1 2 3 4` gives
`1 2 6 24`.

With no axis the array is read flat. Given an axis each line along it accumulates
on its own.

A single zero element zeroes everything after it, and long products of values far
from 1 overflow or underflow quickly — [csncumsum](csncumsum.md) over
[csnlog](csnlog.md) of the magnitudes is the usual way round that.

Both real and complex arrays are accepted.

## Syntax

```csound
handle:CsnArr = csncumprod(source:CsnArr)
handle:CsnArr = csncumprod(source:CsnArr, axis:i)
handle:CsnArr = csncumprod(source:CsnArr, axis:k)
handle:CsnArr = csncumprod(source:CsnArr, axis:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to accumulate.
* `axis:i / axis:k` (optional, default `-1`): the axis to accumulate along; `-1` reads the array flat.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the running products, with the shape of the source.

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
; csncumprod.csd
;
; A running product. Over an array of ratios it turns intervals into absolute
; frequencies, one multiplication per step.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr     = csnfromarray(array(1, 2, 3, 4))
    running:CsnArr  = csncumprod(data)
    running_out:i[] = csntoarray(running)
    prints("cumprod = %g %g %g %g\n", running_out[0], running_out[1], running_out[2], running_out[3])

    ; intervals to a scale: each ratio multiplies the one before
    ratio:CsnArr    = csnfromarray(array(220, 1.125, 1.125, 1.0535))
    freq:CsnArr     = csncumprod(ratio)
    freq_out:i[]    = csntoarray(freq)
    prints("scale   = %.2f %.2f %.2f %.2f\n", freq_out[0], freq_out[1], freq_out[2], freq_out[3])

    ; one zero zeroes everything after it
    holed:CsnArr    = csncumprod(csnfromarray(array(2, 3, 0, 5)))
    holed_out:i[]   = csntoarray(holed)
    prints("with a zero = %g %g %g %g\n", holed_out[0], holed_out[1], holed_out[2], holed_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csncumsum](csncumsum.md)
* [csnprod](csnprod.md)
* [csnlog](csnlog.md)

## Credits

Pasquale Mainolfi, 2026
