# csnlptohpzpk

## Abstract

Turns a lowpass prototype into a highpass, in zeros-poles-gain form.

## Description

`csnlptohpzpk` turns an analog lowpass prototype with a cutoff of 1 rad/s
into a highpass with a cutoff of `w0` rad/s, as `scipy.signal.lp2hp_zpk` does.
The substitution `s -> w0 / s` inverts every root, and each pole in excess of
the zeros brings a zero at the origin:

```
z' = [w0 / z, 0 x (np - nz)]    p' = w0 / p    k' = k * real(prod(-z) / prod(-p))
```

A prototype zero or pole at the origin has no image and is refused.

The zeros and poles may be real or complex and come back complex. The
prototype's zeros may be an empty array, as a Butterworth or Chebyshev type I
prototype has none.

There is no performance-time form.

## Syntax

```csound
zeros:CsnArr, poles:CsnArr, gain:i csnlptohpzpk z:CsnArr, p:CsnArr, k:i, w0:i
```

## Arguments

* `z:CsnArr`: a 1-D real or complex array of the prototype's zeros, possibly empty, none at the origin.
* `p:CsnArr`: a 1-D real or complex array of its poles, at least as many as the zeros, none at the origin.
* `k:i`: the prototype's gain.
* `w0:i`: the cutoff in rad/s, finite and greater than zero.

## Output

* `zeros:CsnArr`: a complex vector of `np` zeros, the mapped ones first.
* `poles:CsnArr`: a complex vector of `np` poles.
* `gain:i`: the new gain.

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
; csnlptohpzpk.csd
;
; Turns a lowpass prototype into a highpass, in zeros-poles-gain form.
; Same arguments and results as scipy.signal.lp2hp_zpk: the zeros and poles come
; back complex, the gain as a scalar.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; analog prototype: one zero at -3, poles at -1, -2, -4, gain 2
    iz[] = fillarray(-3)
    ip[] = fillarray(-1, -2, -4)
    z:CsnArr = csnfromarray(iz)
    p:CsnArr = csnfromarray(ip)

    ; a highpass with its cutoff at 3 rad/s
    zh:CsnArr, ph:CsnArr, igain csnlptohpzpk z, p, 2, 3
    csnprint(zh)
    csnprint(ph)
    prints("gain = %.6f\n", igain)
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
* [csnlptobpzpk](csnlptobpzpk.md)
* [csnlptobszpk](csnlptobszpk.md)
* [csnbilinearzpk](csnbilinearzpk.md)
* [csnroots](csnroots.md)

## Credits

Pasquale Mainolfi, 2026
