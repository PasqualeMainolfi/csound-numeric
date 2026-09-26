# csnlptobs

## Abstract

Turns a lowpass prototype into a bandstop, as transfer-function coefficients.

## Description

`csnlptobs` turns an analog lowpass prototype with a cutoff of 1 rad/s,
given as the coefficients of `b(s) / a(s)`, highest degree first, into a
bandstop centred on `w0` rad/s with a bandwidth of `bw` rad/s, as
`scipy.signal.lp2bs` does. It substitutes

```
s -> s bw / (s^2 + w0^2)
```

and expands the powers binomially, so the order doubles: with `ma` the higher
of the two degrees, both `b` and `a` get `2 ma + 1` coefficients.

The substitution works on the coefficients directly, without finding roots,
so it is exact up to rounding and has no convergence limit. The result is
normalised as `scipy.signal.normalize` does: `a` starts with a 1, its leading
zeros stripped, and the leading coefficients of `b` below `1e-14` are
dropped. The same transform on zeros, poles and gain is
[csnlptobszpk](csnlptobszpk.md).

The coefficients may be real or complex; the result is complex when either
input is. An all-zero `a` is refused.

There is no performance-time form.

## Syntax

```csound
b:CsnArr, a:CsnArr csnlptobs b:CsnArr, a:CsnArr, w0:i, bw:i
```

## Arguments

* `b:CsnArr`: a 1-D real or complex array, the prototype's numerator, highest degree first.
* `a:CsnArr`: a 1-D real or complex array, its denominator, highest degree first.
* `w0:i`: the centre frequency in rad/s, finite and greater than zero.
* `bw:i`: the bandwidth in rad/s, finite and greater than zero.

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
; csnlptobs.csd
;
; Turns a lowpass prototype into a bandstop, as transfer-function coefficients.
; Same arguments and results as scipy.signal.lp2bs.
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

    ; notch out a band 0.5 rad/s wide around 3 rad/s: (s^2 + 9) / (s^2 + 0.5 s + 9)
    bt:CsnArr, at:CsnArr csnlptobs b, a, 3, 0.5
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

* [csnlptobszpk](csnlptobszpk.md)
* [csnlptolp](csnlptolp.md)
* [csnlptohp](csnlptohp.md)
* [csnlptobp](csnlptobp.md)
* [csntftozpk](csntftozpk.md)

## Credits

Pasquale Mainolfi, 2026
