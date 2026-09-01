# csnlinspace

## Abstract

Create a 1-D array of `num` values evenly spaced between two bounds.

## Description

`csnlinspace` produces exactly `num` values from `start` to `stop`, endpoint
**included**, like NumPy's `linspace`. The last element is written as `stop`
rather than accumulated, so the upper bound is exact whatever the rounding along
the way.

With `num = 1` the single element is `start`.

Real only. Use [csnarange](csnarange.md) when the step matters more than the
count.

## Syntax

```csound
handle:CsnArr = csnlinspace(start:i, stop:i, num:i)
handle:CsnArr = csnlinspace(start:k, stop:k, num:k, trig:k)
```

## Arguments

* `start:i / start:k`: first value.
* `stop:i / stop:k`: last value, included.
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
; csnlinspace.csd
;
; csnlinspace gives a fixed number of points between two bounds, endpoint
; included. It is the usual way to build an axis for csninterp.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ramp:CsnArr   = csnlinspace(0, 1, 5)
    ramp_out:i[]  = csntoarray(ramp)
    prints("ramp  = %g %g %g %g %g\n", ramp_out[0], ramp_out[1], ramp_out[2], ramp_out[3], ramp_out[4])

    ; a half period of a sine, sampled at 5 points
    phase:CsnArr  = csnlinspace(0, 3.14159265358979, 5)
    wave:CsnArr   = csnsin(phase)
    wave_out:i[]  = csntoarray(wave)
    prints("sin   = %.3f %.3f %.3f %.3f %.3f\n", wave_out[0], wave_out[1], wave_out[2], wave_out[3], wave_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnarange](csnarange.md)
* [csnlogspace](csnlogspace.md)
* [csngeomspace](csngeomspace.md)
* [csninterp](csninterp.md)

## Credits

Pasquale Mainolfi, 2026
