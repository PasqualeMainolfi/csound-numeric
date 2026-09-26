# csnzpktotf

## Abstract

The coefficients of a transfer function given by its zeros, poles and gain.

## Description

`csnzpktotf` expands a filter given as zeros, poles and gain

```
H(x) = k * prod(x - z) / prod(x - p)
```

into the coefficients of its numerator `b` and denominator `a`, **highest
degree first**, as `scipy.signal.zpk2tf` does:

```
b = k * poly(z)        a = poly(p)
```

`b` has one more coefficient than there are zeros, and `a` one more than there
are poles. `a[0]` is always 1. With no zeros, `b` is `[k]`.

The coefficients come back real when every complex zero or pole has its
conjugate among the others, as they do for any real filter, and complex
otherwise, as scipy returns them. The test is up to rounding: an imaginary part
below `1e-12` of the largest coefficient counts as zero, so the roots found by
[csntftozpk](csntftozpk.md) or [csnroots](csnroots.md) expand back to real
coefficients.

It is the inverse of [csntftozpk](csntftozpk.md), up to the rounding of the
roots and the leading zeros that one strips.

There is no performance-time form.

## Syntax

```csound
b:CsnArr, a:CsnArr csnzpktotf zeros:CsnArr, poles:CsnArr, k:i
```

## Arguments

* `zeros:CsnArr`: a 1-D real or complex array of zeros, possibly empty.
* `poles:CsnArr`: a 1-D real or complex array of poles, possibly empty.
* `k:i`: the gain, finite.

## Output

* `b:CsnArr`: the numerator coefficients, highest degree first, real or complex.
* `a:CsnArr`: the denominator coefficients, highest degree first, real or complex.

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
; csnzpktotf.csd
;
; Zeros, poles and gain turned back into the coefficients of a transfer
; function b / a, highest degree first, as scipy.signal.zpk2tf does:
; b = k * prod(x - z), a = prod(x - p). The coefficients are real when the
; complex roots come in conjugate pairs, and complex otherwise.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; zeros at +-1, poles at -1 and -2, gain 2
    iz[] = fillarray(1, -1)
    ip[] = fillarray(-1, -2)
    z:CsnArr = csnfromarray(iz)
    p:CsnArr = csnfromarray(ip)
    b:CsnArr, a:CsnArr csnzpktotf z, p, 2
    csnprint(b)     ; [2 0 -2]
    csnprint(a)     ; [1 3 2]

    ; the round trip through csntftozpk gives the same filter back
    z2:CsnArr, p2:CsnArr, igain csntftozpk b, a
    b2:CsnArr, a2:CsnArr csnzpktotf z2, p2, igain
    csnprint(b2)
    csnprint(a2)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csntftozpk](csntftozpk.md)
* [csnroots](csnroots.md)
* [csnbilinearzpk](csnbilinearzpk.md)

## Credits

Pasquale Mainolfi, 2026
