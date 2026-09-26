# csntftozpk

## Abstract

The zeros, poles and gain of a transfer function given by its coefficients.

## Description

`csntftozpk` takes a rational transfer function

```
H(s) = b(s) / a(s) = (b[0] s^M + ... + b[M]) / (a[0] s^N + ... + a[N])
```

by its numerator `b` and denominator `a`, coefficients **highest degree
first**, and returns it as zeros, poles and gain, as `scipy.signal.tf2zpk`
does:

```
H(s) = k * prod(s - z) / prod(s - p)
```

The zeros are the roots of `b`, the poles the roots of `a`, found as
[csnroots](csnroots.md) finds them, and the gain is `k = b[0] / a[0]` with its
sign. The same holds for a digital filter in `z`, as long as `b` and `a` are
padded to the same length first, as scipy expects.

Leading zeros only lower the degree and are skipped, as
`scipy.signal.normalize` strips them. Interior and trailing zeros are
coefficients: a trailing zero in `b` or `a` gives a zero or pole at the
origin. A numerator with one non-zero coefficient has no zeros, and the zeros
come back empty.

The coefficients must be real: the gain of a complex transfer function is not
real, and an empty or all-zero `b` or `a` is refused.

There is no performance-time form.

## Syntax

```csound
zeros:CsnArr, poles:CsnArr, gain:i csntftozpk b:CsnArr, a:CsnArr
```

## Arguments

* `b:CsnArr`: a 1-D real array, the numerator coefficients, highest degree first.
* `a:CsnArr`: a 1-D real array, the denominator coefficients, highest degree first.

## Output

* `zeros:CsnArr`: a complex vector of the roots of `b`, possibly empty.
* `poles:CsnArr`: a complex vector of the roots of `a`, possibly empty.
* `gain:i`: `b[0] / a[0]`, over the first non-zero coefficients.

## Execution Time

* Init

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csntftozpk.csd
;
; A transfer function b(s) / a(s), coefficients highest degree first, turned
; into its zeros, poles and gain, as scipy.signal.tf2zpk does. The zeros are
; the roots of b, the poles the roots of a, the gain b[0] / a[0]. Leading zeros
; only lower the degree and are skipped.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; (2s^2 - 2) / (s^2 + 3s + 2) = 2 (s - 1)(s + 1) / ((s + 1)(s + 2))
    ib[] = fillarray(2, 0, -2)
    ia[] = fillarray(1, 3, 2)
    b:CsnArr = csnfromarray(ib)
    a:CsnArr = csnfromarray(ia)
    z:CsnArr, p:CsnArr, igain csntftozpk b, a
    csnprint(z)
    csnprint(p)
    prints("gain = %.6f\n", igain)

    ; an all-pole filter has no zeros: the zeros come back empty
    ib2[] = fillarray(1)
    b2:CsnArr = csnfromarray(ib2)
    z2:CsnArr, p2:CsnArr, igain2 csntftozpk b2, a
    csnprint(z2)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnroots](csnroots.md)
* [csnbilinearzpk](csnbilinearzpk.md)
* [csnlptolpzpk](csnlptolpzpk.md)

## Credits

Pasquale Mainolfi, 2026
