# csnpow

## Abstract

Elementwise power of two arrays, or of an array and a scalar.

## Description

`csnpow` raises its first operand to the power of its second, elementwise. Two
arrays are broadcast against each other NumPy-style — aligned from the last axis,
each pair of extents must either match or be 1.

Both scalar orders exist. `csnpow(array, value)` raises every element to a fixed
exponent, which covers squares, roots and the usual curve shaping;
`csnpow(value, array)` raises a fixed base to every element, which is the
exponential over an arbitrary base.

Real and complex arrays are both accepted, and an operation mixing the two
promotes the result to complex.

## Syntax

```csound
handle:CsnArr = csnpow(a:CsnArr, b:CsnArr)
handle:CsnArr = csnpow(a:CsnArr, b:CsnArr, trig:k)
handle:CsnArr = csnpow(a:CsnArr, value:i)
handle:CsnArr = csnpow(a:CsnArr, value:k)
handle:CsnArr = csnpow(a:CsnArr, value:k, trig:k)
handle:CsnArr = csnpow(a:CsnArr, value:Complex)
handle:CsnArr = csnpow(a:CsnArr, value:Complex, trig:k)
handle:CsnArr = csnpow(value:i, b:CsnArr)
handle:CsnArr = csnpow(value:k, b:CsnArr)
handle:CsnArr = csnpow(value:k, b:CsnArr, trig:k)
handle:CsnArr = csnpow(value:Complex, b:CsnArr)
handle:CsnArr = csnpow(value:Complex, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`: the base.
* `b:CsnArr`: the exponent; broadcast against `a`.
* `value:i / value:k / value:Complex`: a scalar, on either side of the operation.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the result.

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
; csnpow.csd
;
; The scalar on the right is a fixed exponent, which is how curves are shaped;
; the scalar on the left is a fixed base, which is an exponential.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    base:CsnArr    = csnfromarray(array(1, 2, 3, 4))
    expo:CsnArr    = csnfromarray(array(2, 2, 3, 0.5))

    both:CsnArr    = csnpow(base, expo)
    both_out:i[]   = csntoarray(both)
    prints("array pow array = %g %g %g %g\n", both_out[0], both_out[1], both_out[2], both_out[3])

    ; a fixed exponent shapes a curve
    ramp:CsnArr    = csnlinspace(0, 1, 5)
    curve:CsnArr   = csnpow(ramp, 2)
    curve_out:i[]  = csntoarray(curve)
    prints("squared ramp    = %g %g %g %g %g\n", curve_out[0], curve_out[1], curve_out[2], curve_out[3], curve_out[4])

    ; a fixed base is an exponential
    steps:CsnArr   = csnarange(0, 5, 1)
    powers:CsnArr  = csnpow(2, steps)
    powers_out:i[] = csntoarray(powers)
    prints("2 pow array     = %g %g %g %g %g\n", powers_out[0], powers_out[1], powers_out[2], powers_out[3], powers_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnlog](csnlog.md)
* [csnexp](csnexp.md)
* [csnsqrt](csnsqrt.md)
* [csnmul](csnmul.md)

## Credits

Pasquale Mainolfi, 2026
