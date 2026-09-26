# csncheby1ba

## Abstract

A digital Chebyshev type I lowpass, highpass, bandpass or bandstop filter, as transfer-function coefficients.

## Description

`csncheby1ba` designs a digital Chebyshev type I filter and returns it as
the coefficients of its numerator `b` and denominator `a`, highest degree
first, as `scipy.signal.cheby1` does with `output='ba'`. The filter is
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
5. the zeros and poles expanded into `b` and `a`.

**Prefer [csncheby1sos](csncheby1sos.md) beyond the lowest orders.** A single `b / a`
loses precision as the order grows, the more so for narrow bands, and the
sharper responses of this family put its poles closer to the unit circle than
a Butterworth filter's.

The arguments follow the other csnum designs, order and cutoff first, then
`rp` after the cutoff, then the type and the sampling rate; scipy puts the ripple
before the cutoff. The order must be an integer of at least one. The cutoffs
must lie strictly between 0 and `fs / 2`, and for a band the low one below the
high one. A single cutoff is only for a lowpass or a highpass, two only for a
bandpass or a bandstop.

There is no performance-time form.

## Syntax

```csound
b:CsnArr, a:CsnArr csncheby1ba order:i, fc:i, rp:i, type:i, fs:i
b:CsnArr, a:CsnArr csncheby1ba order:i, band:i[], rp:i, type:i, fs:i
```

## Arguments

* `order:i`: the filter order, an integer greater or equal to one.
* `fc:i`: the cutoff in Hz, for a lowpass or a highpass.
* `band:i[]`: the two band edges in Hz, low then high, for a bandpass or a bandstop. An inline array passed at global scope arrives empty: bind it to a named i-array first.
* `rp:i`: the passband ripple in dB, finite and greater than zero.
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
; csncheby1ba.csd
;
; Digital Chebyshev type I filters, as scipy.signal.cheby1 with output='ba'.
; One cutoff makes a lowpass (type 0) or a highpass (1), an array of two a
; bandpass (2) or a bandstop (3). Frequencies are in Hz at the sampling rate
; given last.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    ; sixth-order lowpass at 1 kHz, 1 dB of ripple
    b:CsnArr, a:CsnArr csncheby1ba 6, 1000, 1, 0, sr
    csnprint(b)
    csnprint(a)

    ; fourth-order bandpass, 300 Hz to 3 kHz, 0.5 dB of ripple: the band goes in a named array
    iband[] = fillarray(300, 3000)
    bb:CsnArr, ab:CsnArr csncheby1ba 4, iband, 0.5, 2, sr
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

* [csncheby1sos](csncheby1sos.md)
* [csnbutterba](csnbutterba.md)
* [csnzpktosos](csnzpktosos.md)

## Credits

Pasquale Mainolfi, 2026
