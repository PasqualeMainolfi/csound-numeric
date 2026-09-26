# csnlptolp

## Abstract

Moves the cutoff of a lowpass prototype, as transfer-function coefficients.

## Description

`csnlptolp` turns an analog lowpass prototype with a cutoff of 1 rad/s,
given as the coefficients of `b(s) / a(s)`, highest degree first, into a
lowpass with a cutoff of `w0` rad/s, as `scipy.signal.lp2lp` does. It
substitutes `s -> s / w0`:

```
H'(s) = H(s / w0)
```

which scales the coefficient of `s^i` by `w0^-i`. The result keeps the lengths
of `b` and `a`.

The substitution works on the coefficients directly, without finding roots,
so it is exact up to rounding and has no convergence limit. The result is
normalised as `scipy.signal.normalize` does: `a` starts with a 1, its leading
zeros stripped, and the leading coefficients of `b` below `1e-14` are
dropped. The same transform on zeros, poles and gain is
[csnlptolpzpk](csnlptolpzpk.md).

The coefficients may be real or complex; the result is complex when either
input is. An all-zero `a` is refused.

There is no performance-time form.

## Syntax

```csound
b:CsnArr, a:CsnArr csnlptolp b:CsnArr, a:CsnArr, w0:i
```

## Arguments

* `b:CsnArr`: a 1-D real or complex array, the prototype's numerator, highest degree first.
* `a:CsnArr`: a 1-D real or complex array, its denominator, highest degree first.
* `w0:i`: the cutoff in rad/s, finite and greater than zero.

## Output

* `b:CsnArr`: the numerator of the transformed filter, highest degree first.
* `a:CsnArr`: its denominator, highest degree first, `a[0] = 1`.

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
; csnlptolp.csd
;
; Moves the cutoff of a lowpass prototype, as transfer-function coefficients.
; Same arguments and results as scipy.signal.lp2lp.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; first-order lowpass prototype 1 / (s + 1)
    ib[] = fillarray(1)
    ia[] = fillarray(1, 1)
    b:CsnArr = csnfromarray(ib)
    a:CsnArr = csnfromarray(ia)

    ; move the cutoff to 3 rad/s: 3 / (s + 3)
    bt:CsnArr, at:CsnArr csnlptolp b, a, 3
    csnprint(bt)
    csnprint(at)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnlptolpzpk](csnlptolpzpk.md)
* [csnlptohp](csnlptohp.md)
* [csnlptobp](csnlptobp.md)
* [csnlptobs](csnlptobs.md)
* [csntftozpk](csntftozpk.md)

## Credits

Pasquale Mainolfi, 2026
