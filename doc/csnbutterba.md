# csnbutterba

## Abstract

A digital Butterworth lowpass, highpass, bandpass or bandstop filter, as transfer-function coefficients.

## Description

`csnbutterba` designs a digital Butterworth filter and returns the coefficients
of its numerator `b` and denominator `a`, highest degree first, as
`scipy.signal.butter` does with `output='ba'`. A Butterworth filter is
maximally flat in the passband and falls monotonically, and it is 3 dB down,
`|H| = 1/sqrt(2)`, exactly at each cutoff.

The design follows scipy's steps:

1. the analog prototype of order `N`, as [csnbuttap](csnbuttap.md) gives it;
2. each cutoff `f` pre-warped to `2 fs tan(pi f / fs)` rad/s, so the bilinear
   transform lands it back on `f`;
3. the prototype moved to the requested type, as
   [csnlptolpzpk](csnlptolpzpk.md), [csnlptohpzpk](csnlptohpzpk.md),
   [csnlptobpzpk](csnlptobpzpk.md) or [csnlptobszpk](csnlptobszpk.md) do, the
   band ones centred on `sqrt(w1 w2)` with bandwidth `w2 - w1`;
4. the bilinear transform at `fs`, as [csnbilinearzpk](csnbilinearzpk.md);
5. the zeros and poles expanded into `b` and `a`.

A lowpass or highpass has `N + 1` coefficients in each of `b` and `a`, a
bandpass or bandstop `2N + 1`.

**Prefer [csnbuttersos](csnbuttersos.md) beyond the lowest orders.** The
coefficients of a single `b / a` lose precision as the order grows, the more
so for narrow bands and cutoffs far below `fs / 2`, where the poles crowd near
`z = 1`: a sixth-order bandpass from 500 to 700 Hz at 48 kHz already has a
denominator root outside the unit circle as `b, a`, and is stable as
sections.

The order must be an integer of at least one. The cutoffs must lie strictly
between 0 and `fs / 2`, and for a band the low one below the high one. A
single cutoff is only for a lowpass or a highpass, two only for a bandpass or
a bandstop.

There is no performance-time form.

## Syntax

```csound
b:CsnArr, a:CsnArr csnbutterba order:i, fc:i, type:i, fs:i
b:CsnArr, a:CsnArr csnbutterba order:i, band:i[], type:i, fs:i
```

## Arguments

* `order:i`: the filter order, an integer greater or equal to one.
* `fc:i`: the cutoff in Hz, for a lowpass or a highpass.
* `band:i[]`: the two band edges in Hz, low then high, for a bandpass or a bandstop. An inline array passed at global scope arrives empty: bind it to a named i-array first.
* `type:i`: `0` lowpass, `1` highpass, `2` bandpass, `3` bandstop.
* `fs:i`: the sampling rate in Hz, a positive integer.

## Output

* `b:CsnArr`: the numerator coefficients, highest degree first.
* `a:CsnArr`: the denominator coefficients, highest degree first, `a[0] = 1`.

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
; csnbutterba.csd
;
; Digital Butterworth filters, as scipy.signal.butter with output='ba':
; maximally flat in the passband and 3 dB down exactly at the cutoff. One
; cutoff makes a lowpass (type 0) or a highpass (1), an array of two a
; bandpass (2) or a bandstop (3). Frequencies are in Hz at the sampling rate
; given last.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; second-order lowpass at a quarter of the sampling rate:
    ; b = [0.2929 0.5858 0.2929], a = [1 0 0.1716]
    b:CsnArr, a:CsnArr csnbutterba 2, sr / 4, 0, sr
    csnprint(b)
    csnprint(a)

    ; fourth-order highpass at 100 Hz
    bh:CsnArr, ah:CsnArr csnbutterba 4, 100, 1, sr
    csnprint(bh)
    csnprint(ah)

    ; second-order bandpass, 300 Hz to 3 kHz: the band goes in a named array
    iband[] = fillarray(300, 3000)
    bb:CsnArr, ab:CsnArr csnbutterba 2, iband, 2, sr
    csnprint(bb)
    csnprint(ab)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnbuttersos](csnbuttersos.md)
* [csnbuttap](csnbuttap.md)
* [csnbilinearzpk](csnbilinearzpk.md)
* [csnzpktosos](csnzpktosos.md)
* [csntftozpk](csntftozpk.md)

## Credits

Pasquale Mainolfi, 2026
