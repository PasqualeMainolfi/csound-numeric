# csngrad

## Abstract

Central-difference gradient.

## Description

`csngrad` estimates the derivative at every point and, unlike
[csndiff](csndiff.md), **keeps the array's length**. In the interior it uses the
central difference `(next - previous) / 2`, which is second-order accurate; at
the two ends, where there is no neighbour on one side, it falls back to the
one-sided difference.

Because the sample count is preserved, the result lines up with the source
element by element, which is what makes it the one to use when the derivative has
to be multiplied back into the original data.

With no axis the array is read flat. Given an axis each line along it is
differentiated on its own.

Real only.

## Syntax

```csound
handle:CsnArr = csngrad(source:CsnArr)
handle:CsnArr = csngrad(source:CsnArr, axis:i)
handle:CsnArr = csngrad(source:CsnArr, axis:k)
handle:CsnArr = csngrad(source:CsnArr, axis:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to differentiate.
* `axis:i / axis:k` (optional, default `-1`): the axis to work along; `-1` reads the array flat.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the gradient, with the shape of the source.

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
; csngrad.csd
;
; Same length in, same length out. That is what separates it from csndiff, and
; what lets the gradient be lined up with the data it came from.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(1, 4, 9, 16))

    slope:CsnArr  = csngrad(data)
    slope_out:i[] = csntoarray(slope)
    slope_n:i     = csnsize(slope)
    prints("grad n = %d : %g %g %g %g\n", slope_n, slope_out[0], slope_out[1], slope_out[2], slope_out[3])

    ; csndiff answers the same question one element shorter
    steps:CsnArr  = csndiff(data)
    steps_n:i     = csnsize(steps)
    prints("diff n = %d\n", steps_n)

    ; the gradient of a straight line is constant
    ramp:CsnArr   = csnlinspace(0, 4, 5)
    flat:CsnArr   = csngrad(ramp)
    flat_out:i[]  = csntoarray(flat)
    prints("gradient of a ramp = %g %g %g %g %g\n", flat_out[0], flat_out[1], flat_out[2], flat_out[3], flat_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csndiff](csndiff.md)
* [csncumsum](csncumsum.md)
* [csnmovmean](csnmovmean.md)

## Credits

Pasquale Mainolfi, 2026
