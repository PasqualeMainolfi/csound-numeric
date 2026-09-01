# csnlogspace

## Abstract

Create a 1-D array of `num` values evenly spaced on a logarithmic scale.

## Description

`csnlogspace` produces `num` values of the form `base^x`, where `x` runs evenly
from `start` to `stop`, endpoint included — NumPy's `logspace`. The bounds are
**exponents**, not values: `csnlogspace(1, 4, 4, 10)` gives `10 100 1000 10000`.

`base` must be greater than 0. With `num = 1` the single element is
`base^start`.

Real only. When you would rather give the first and last *values* than their
exponents, use [csngeomspace](csngeomspace.md).

## Syntax

```csound
handle:CsnArr = csnlogspace(start:i, stop:i, num:i, base:i)
handle:CsnArr = csnlogspace(start:k, stop:k, num:k, base:k, trig:k)
```

## Arguments

* `start:i / start:k`: exponent of the first value.
* `stop:i / stop:k`: exponent of the last value, included.
* `num:i / num:k`: number of elements.
* `base:i / base:k`: the base the exponents are raised over; must be > 0.
* `trig:k`: k-rate trigger. The array is rebuilt on a non-zero trigger; a zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the new 1-D array of `num` elements.

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
; csnlogspace.csd
;
; csnlogspace spaces exponents evenly, then raises the base over them. Handy for
; a decade-wise frequency axis: the bounds are exponents, not frequencies.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    dec:CsnArr    = csnlogspace(1, 4, 4, 10)
    dec_out:i[]   = csntoarray(dec)
    prints("decades = %g %g %g %g\n", dec_out[0], dec_out[1], dec_out[2], dec_out[3])

    ; four octaves above 55 Hz, in base 2
    oct:CsnArr    = csnlogspace(0, 4, 5, 2)
    freq:CsnArr   = csnmul(oct, 55)
    freq_out:i[]  = csntoarray(freq)
    prints("octaves = %g %g %g %g %g\n", freq_out[0], freq_out[1], freq_out[2], freq_out[3], freq_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csngeomspace](csngeomspace.md)
* [csnlinspace](csnlinspace.md)
* [csnarange](csnarange.md)

## Credits

Pasquale Mainolfi, 2026
