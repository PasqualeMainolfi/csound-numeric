# csngeomspace

## Abstract

Create a 1-D array of `num` values in geometric progression.

## Description

`csngeomspace` produces `num` values from `start` to `stop`, endpoint included,
each one a fixed ratio above the previous — NumPy's `geomspace`. The ratio is
`(stop / start)^(1 / (num - 1))`, so both bounds are given as **values**, not as
exponents.

`start` must not be zero, and `start` and `stop` must share a sign, since no real
ratio bridges them otherwise. With `num = 1` the single element is `start`.

Real only. Use [csnlogspace](csnlogspace.md) when it is more natural to state the
exponents than the endpoints.

## Syntax

```csound
handle:CsnArr = csngeomspace(start:i, stop:i, num:i)
handle:CsnArr = csngeomspace(start:k, stop:k, num:k, trig:k)
```

## Arguments

* `start:i / start:k`: first value; must be non-zero.
* `stop:i / stop:k`: last value, included; must have the same sign as `start`.
* `num:i / num:k`: number of elements.
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
; csngeomspace.csd
;
; csngeomspace fixes the two endpoints and finds the constant ratio between
; them. Five points from 100 Hz to 1600 Hz is four octaves, one per step.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    geo:CsnArr   = csngeomspace(100, 1600, 5)
    geo_out:i[]  = csntoarray(geo)
    prints("values = %g %g %g %g %g\n", geo_out[0], geo_out[1], geo_out[2], geo_out[3], geo_out[4])

    ratio:i      = geo_out[1] / geo_out[0]
    prints("ratio  = %g\n", ratio)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnlogspace](csnlogspace.md)
* [csnlinspace](csnlinspace.md)
* [csnarange](csnarange.md)

## Credits

Pasquale Mainolfi, 2026
