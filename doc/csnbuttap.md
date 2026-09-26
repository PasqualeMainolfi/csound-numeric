# csnbuttap

## Abstract

The analog Butterworth lowpass prototype, as zeros, poles and gain.

## Description

`csnbuttap` returns the analog Butterworth lowpass prototype of order `N`,
with its cutoff at 1 rad/s, as `scipy.signal.buttap` does: no zeros, gain 1,
and `N` poles evenly spaced on the left half of the unit circle,

```
p[k] = -exp(j pi m / 2N)        m = -N+1, -N+3, ..., N-1
```

in that order. The formula is symmetric in `m`, so the complex poles come in
exact conjugate pairs and the real pole of an odd order is exactly `-1`.

It is the starting point of a Butterworth design: move the cutoff or turn it
into a highpass, bandpass or bandstop with [csnlptolpzpk](csnlptolpzpk.md) and
its siblings, make it digital with [csnbilinearzpk](csnbilinearzpk.md) (after
pre-warping the frequencies with `2 fs tan(pi f / fs)`), and split it into
sections with [csnzpktosos](csnzpktosos.md).

The order must be a non-negative integer. Order 0 gives no poles, a constant
gain of 1.

There is no performance-time form.

## Syntax

```csound
zeros:CsnArr, poles:CsnArr, gain:i csnbuttap order:i
```

## Arguments

* `order:i`: the filter order, a non-negative integer.

## Output

* `zeros:CsnArr`: an empty complex array.
* `poles:CsnArr`: a complex vector of `order` poles.
* `gain:i`: 1.

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
; csnbuttap.csd
;
; The analog Butterworth lowpass prototype of order N, cutoff 1 rad/s, as
; zeros, poles and gain, like scipy.signal.buttap: no zeros, N poles evenly
; spaced on the left half of the unit circle, gain 1. The zpk transforms move
; it to the band wanted, and the bilinear transform makes it digital.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; order 3: poles at -0.5 +- 0.866j and -1
    z:CsnArr, p:CsnArr, ik csnbuttap 3
    csnprint(z)
    csnprint(p)
    prints("gain = %g\n", ik)

    ; a digital lowpass at 1 kHz: move the cutoff, pre-warped, then bilinear
    ifs = 44100
    iwc = 2 * ifs * tan($M_PI * 1000 / ifs)
    zl:CsnArr, pl:CsnArr, ikl csnlptolpzpk z, p, ik, iwc
    zd:CsnArr, pd:CsnArr, ikd csnbilinearzpk zl, pl, ikl, ifs
    sos:CsnArr = csnzpktosos(zd, pd, ikd)
    csnprint(sos)
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
* [csnbilinearzpk](csnbilinearzpk.md)
* [csnzpktosos](csnzpktosos.md)

## Credits

Pasquale Mainolfi, 2026
