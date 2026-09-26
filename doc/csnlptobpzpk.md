# csnlptobpzpk

## Abstract

Turns a lowpass prototype into a bandpass, in zeros-poles-gain form.

## Description

`csnlptobpzpk` turns an analog lowpass prototype with a cutoff of 1 rad/s
into a bandpass centred on `w0` rad/s with a bandwidth of `bw` rad/s, as
`scipy.signal.lp2bp_zpk` does. Every root `r` of the prototype becomes the pair

```
x +- sqrt(x^2 - w0^2)        with x = r * bw / 2
```

so the order doubles. Each pole in excess of the zeros brings a zero at the
origin, and the gain becomes `k * bw^(np - nz)`. The two roots of a pair are
stored next to each other.

The zeros and poles may be real or complex and come back complex. The
prototype's zeros may be an empty array, as a Butterworth or Chebyshev type I
prototype has none.

There is no performance-time form.

## Syntax

```csound
zeros:CsnArr, poles:CsnArr, gain:i csnlptobpzpk z:CsnArr, p:CsnArr, k:i, w0:i, bw:i
```

## Arguments

* `z:CsnArr`: a 1-D real or complex array of the prototype's zeros, possibly empty.
* `p:CsnArr`: a 1-D real or complex array of its poles, at least as many as the zeros.
* `k:i`: the prototype's gain.
* `w0:i`: the centre frequency in rad/s, finite and greater than zero.
* `bw:i`: the bandwidth in rad/s, finite and greater than zero.

## Output

* `zeros:CsnArr`: a complex vector of `2 nz + (np - nz)` zeros, the mapped pairs first.
* `poles:CsnArr`: a complex vector of `2 np` poles.
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
; csnlptobpzpk.csd
;
; Turns a lowpass prototype into a bandpass, in zeros-poles-gain form.
; Same arguments and results as scipy.signal.lp2bp_zpk: the zeros and poles come
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

    ; a band 0.5 rad/s wide around 3 rad/s
    zb:CsnArr, pb:CsnArr, igain csnlptobpzpk z, p, 2, 3, 0.5
    csnprint(zb)
    csnprint(pb)
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
* [csnlptohpzpk](csnlptohpzpk.md)
* [csnlptobszpk](csnlptobszpk.md)
* [csnbilinearzpk](csnbilinearzpk.md)
* [csnroots](csnroots.md)

## Credits

Pasquale Mainolfi, 2026
