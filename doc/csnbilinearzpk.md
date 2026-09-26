# csnbilinearzpk

## Abstract

Maps an analog filter to a digital one by the bilinear transform, in zeros-poles-gain form.

## Description

`csnbilinearzpk` maps the zeros, poles and gain of an analog filter to those
of a digital one by the bilinear transform `s = 2 fs (z - 1) / (z + 1)`, as
`scipy.signal.bilinear_zpk` does. With `fs2 = 2 fs`:

```
z' = [(fs2 + z) / (fs2 - z), -1 x (np - nz)]    p' = (fs2 + p) / (fs2 - p)
k' = k * real(prod(fs2 - z) / prod(fs2 - p))
```

Each pole in excess of the zeros brings a zero at `-1`, Nyquist. The
transform warps the frequency axis: an analog frequency `w` lands at
`2 atan(w / fs2)` rad/sample, so a design meant to hit a digital frequency
exactly should pre-warp its `w0` with `fs2 * tan(wd / 2)` first.

A root at exactly `fs2` has no image and is refused. A stable analog filter,
every pole in the left half plane, gives a stable digital one, every pole
inside the unit circle.

The zeros and poles may be real or complex and come back complex. The
prototype's zeros may be an empty array, as a Butterworth or Chebyshev type I
prototype has none.

There is no performance-time form.

## Syntax

```csound
zeros:CsnArr, poles:CsnArr, gain:i csnbilinearzpk z:CsnArr, p:CsnArr, k:i, fs:i
```

## Arguments

* `z:CsnArr`: a 1-D real or complex array of the analog zeros, possibly empty.
* `p:CsnArr`: a 1-D real or complex array of the analog poles, at least as many as the zeros.
* `k:i`: the analog gain.
* `fs:i`: the sampling rate in Hz, finite and greater than zero.

## Output

* `zeros:CsnArr`: a complex vector of `np` digital zeros, the mapped ones first.
* `poles:CsnArr`: a complex vector of `np` digital poles.
* `gain:i`: the digital gain.

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
; csnbilinearzpk.csd
;
; Maps an analog filter to a digital one by the bilinear transform, in zeros-poles-gain form.
; Same arguments and results as scipy.signal.bilinear_zpk: the zeros and poles come
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

    ; to digital, at a sampling rate of 10 Hz
    zd:CsnArr, pd:CsnArr, igain csnbilinearzpk z, p, 2, 10
    csnprint(zd)
    csnprint(pd)
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
* [csnlptobszpk](csnlptobszpk.md)
* [csnroots](csnroots.md)

## Credits

Pasquale Mainolfi, 2026
