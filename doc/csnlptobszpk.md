# csnlptobszpk

## Abstract

Turns a lowpass prototype into a bandstop, in zeros-poles-gain form.

## Description

`csnlptobszpk` turns an analog lowpass prototype with a cutoff of 1 rad/s
into a bandstop centred on `w0` rad/s with a bandwidth of `bw` rad/s, as
`scipy.signal.lp2bs_zpk` does. Every root `r` of the prototype becomes the pair

```
x +- sqrt(x^2 - w0^2)        with x = (bw / 2) / r
```

so the order doubles. Each pole in excess of the zeros brings a zero at
`+j w0` and one at `-j w0`, all the `+j w0` ones first, which is where the
stopband is notched out. The gain becomes `k * real(prod(-z) / prod(-p))`.
A prototype zero or pole at the origin has no image and is refused.

The zeros and poles may be real or complex and come back complex. The
prototype's zeros may be an empty array, as a Butterworth or Chebyshev type I
prototype has none.

There is no performance-time form.

## Syntax

```csound
zeros:CsnArr, poles:CsnArr, gain:i csnlptobszpk z:CsnArr, p:CsnArr, k:i, w0:i, bw:i
```

## Arguments

* `z:CsnArr`: a 1-D real or complex array of the prototype's zeros, possibly empty, none at the origin.
* `p:CsnArr`: a 1-D real or complex array of its poles, at least as many as the zeros, none at the origin.
* `k:i`: the prototype's gain.
* `w0:i`: the centre frequency in rad/s, finite and greater than zero.
* `bw:i`: the bandwidth in rad/s, finite and greater than zero.

## Output

* `zeros:CsnArr`: a complex vector of `2 np` zeros, the mapped pairs first.
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
; csnlptobszpk.csd
;
; Turns a lowpass prototype into a bandstop, in zeros-poles-gain form.
; Same arguments and results as scipy.signal.lp2bs_zpk: the zeros and poles come
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

    ; notch out a band 0.5 rad/s wide around 3 rad/s
    zs:CsnArr, ps:CsnArr, igain csnlptobszpk z, p, 2, 3, 0.5
    csnprint(zs)
    csnprint(ps)
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
* [csnlptobpzpk](csnlptobpzpk.md)
* [csnbilinearzpk](csnbilinearzpk.md)
* [csnroots](csnroots.md)

## Credits

Pasquale Mainolfi, 2026
