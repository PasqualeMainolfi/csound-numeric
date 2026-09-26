# csncheby1sos

## Abstract

A digital Chebyshev type I lowpass, highpass, bandpass or bandstop filter, as second-order sections.

## Description

`csncheby1sos` designs a digital Chebyshev type I filter and returns it as
a cascade of second-order sections, as `scipy.signal.cheby1` does with
`output='sos'`: an `(n, 6)` array with one `[b0 b1 b2 a0 a1 a2]` row per
section, paired as [csnzpktosos](csnzpktosos.md) pairs them. The filter is
equiripple in the passband, between 1 and `10^(-rp/20)`, and monotonic in
the stopband. It trades the flat passband of a Butterworth filter for a
steeper transition at the same order, and at each cutoff the gain is exactly
`10^(-rp/20)`, the bottom of the ripple, not 3 dB down.

The design follows scipy's steps, the same as [csnbutterba](csnbutterba.md)
with a different prototype:

1. the analog lowpass prototype of order `N`: `N` poles on an ellipse, `-sinh(mu + j theta)` with
   `mu = asinh(1 / eps) / N` and `eps = sqrt(10^(rp/10) - 1)`, no zeros, as
   `scipy.signal.cheb1ap` gives them;
2. each cutoff `f` pre-warped to `2 fs tan(pi f / fs)` rad/s;
3. the prototype moved to the requested type, the band ones centred on
   `sqrt(w1 w2)` with bandwidth `w2 - w1`;
4. the bilinear transform at `fs`;
5. the zeros and poles expanded into second-order sections.

This is the form to use beyond the lowest orders: each section holds one
pair of poles, so it stays as accurate as the poles themselves where a single
`b / a` loses precision.

The arguments follow the other csnum designs, order and cutoff first, then
`rp` after the cutoff, then the type and the sampling rate; scipy puts the ripple
before the cutoff. The order must be an integer of at least one. The cutoffs
must lie strictly between 0 and `fs / 2`, and for a band the low one below the
high one. A single cutoff is only for a lowpass or a highpass, two only for a
bandpass or a bandstop.

There is no performance-time form.

## Syntax

```csound
sos:CsnArr = csncheby1sos(order:i, fc:i, rp:i, type:i, fs:i)
sos:CsnArr = csncheby1sos(order:i, band:i[], rp:i, type:i, fs:i)
```

## Arguments

* `order:i`: the filter order, an integer greater or equal to one.
* `fc:i`: the cutoff in Hz, for a lowpass or a highpass.
* `band:i[]`: the two band edges in Hz, low then high, for a bandpass or a bandstop. An inline array passed at global scope arrives empty: bind it to a named i-array first.
* `rp:i`: the passband ripple in dB, finite and greater than zero.
* `type:i`: `0` lowpass, `1` highpass, `2` bandpass, `3` bandstop.
* `fs:i`: the sampling rate in Hz, a positive integer.

## Output

* `sos:CsnArr`: a real `(n, 6)` array of second-order sections, `[b0 b1 b2 a0 a1 a2]` per row, `a0 = 1`.

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
; csncheby1sos.csd
;
; Digital Chebyshev type I filters, as scipy.signal.cheby1 with output='sos'.
; One cutoff makes a lowpass (type 0) or a highpass (1), an array of two a
; bandpass (2) or a bandstop (3). Frequencies are in Hz at the sampling rate
; given last.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    ; sixth-order lowpass at 1 kHz, 1 dB of ripple
    sos:CsnArr = csncheby1sos(6, 1000, 1, 0, sr)
    csnprint(sos)

    ; fourth-order bandpass, 300 Hz to 3 kHz, 0.5 dB of ripple: the band goes in a named array
    iband[] = fillarray(300, 3000)
    sosb:CsnArr = csncheby1sos(4, iband, 0.5, 2, sr)
    csnprint(sosb)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csncheby1ba](csncheby1ba.md)
* [csnbuttersos](csnbuttersos.md)
* [csnzpktosos](csnzpktosos.md)

## Credits

Pasquale Mainolfi, 2026
