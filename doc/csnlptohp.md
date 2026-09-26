# csnlptohp

## Abstract

Turns a lowpass prototype into a highpass, as transfer-function coefficients.

## Description

`csnlptohp` turns an analog lowpass prototype with a cutoff of 1 rad/s,
given as the coefficients of `b(s) / a(s)`, highest degree first, into a
highpass with a cutoff of `w0` rad/s, as `scipy.signal.lp2hp` does. It
substitutes `s -> w0 / s`:

```
H'(s) = H(w0 / s)
```

which reverses both polynomials and pads them to the same length. Unlike
[csnlptohpzpk](csnlptohpzpk.md), a root at the origin is fine here.

The substitution works on the coefficients directly, without finding roots,
so it is exact up to rounding and has no convergence limit. The result is
normalised as `scipy.signal.normalize` does: `a` starts with a 1, its leading
zeros stripped, and the leading coefficients of `b` below `1e-14` are
dropped. The same transform on zeros, poles and gain is
[csnlptohpzpk](csnlptohpzpk.md).

The coefficients may be real or complex; the result is complex when either
input is. An all-zero `a` is refused.

There is no performance-time form.

## Syntax

```csound
b:CsnArr, a:CsnArr csnlptohp b:CsnArr, a:CsnArr, w0:i
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
; csnlptohp.csd
;
; Turns a lowpass prototype into a highpass, as transfer-function coefficients.
; Same arguments and results as scipy.signal.lp2hp.
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

    ; a highpass with its cutoff at 3 rad/s: s / (s + 3)
    bt:CsnArr, at:CsnArr csnlptohp b, a, 3
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

* [csnlptohpzpk](csnlptohpzpk.md)
* [csnlptolp](csnlptolp.md)
* [csnlptobp](csnlptobp.md)
* [csnlptobs](csnlptobs.md)
* [csntftozpk](csntftozpk.md)

## Credits

Pasquale Mainolfi, 2026
