# csnbuttersos

## Abstract

A digital Butterworth lowpass, highpass, bandpass or bandstop filter, as second-order sections.

## Description

`csnbuttersos` designs the same digital Butterworth filter as
[csnbutterba](csnbutterba.md), and returns it as a cascade of second-order
sections, as `scipy.signal.butter` does with `output='sos'`. The result is an
`(n, 6)` array, one section per row:

```
[b0 b1 b2 a0 a1 a2]        H_i(z) = (b0 + b1 z^-1 + b2 z^-2) / (a0 + a1 z^-1 + a2 z^-2)
```

with `a0 = 1`. The filter is the product of the sections. A lowpass or
highpass of order `N` has `ceil(N / 2)` sections, a bandpass or bandstop `N`.

**This is the form to use beyond the lowest orders.** Expanding every pole
into one polynomial `a` amplifies rounding: a sixth-order bandpass from 500 to
700 Hz at 48 kHz is stable as sections, while as `b, a` its denominator has a
root at radius 1.049, outside the unit circle, and the filter blows up. Each
section only holds one pair of poles, so it stays as accurate as the poles
themselves.

The design is the one [csnbutterba](csnbutterba.md) describes: the prototype
of [csnbuttap](csnbuttap.md), the cutoffs pre-warped, the transform of the
type, the bilinear transform. The zeros and poles are then paired into
sections as [csnzpktosos](csnzpktosos.md) pairs them: from the unit circle
inwards, the sharpest section last, the gain in the first.

The order must be an integer of at least one. The cutoffs must lie strictly
between 0 and `fs / 2`, and for a band the low one below the high one. A
single cutoff is only for a lowpass or a highpass, two only for a bandpass or
a bandstop.

There is no performance-time form.

## Syntax

```csound
sos:CsnArr = csnbuttersos(order:i, fc:i, type:i, fs:i)
sos:CsnArr = csnbuttersos(order:i, band:i[], type:i, fs:i)
```

## Arguments

* `order:i`: the filter order, an integer greater or equal to one.
* `fc:i`: the cutoff in Hz, for a lowpass or a highpass.
* `band:i[]`: the two band edges in Hz, low then high, for a bandpass or a bandstop. An inline array passed at global scope arrives empty: bind it to a named i-array first.
* `type:i`: `0` lowpass, `1` highpass, `2` bandpass, `3` bandstop.
* `fs:i`: the sampling rate in Hz, a positive integer.

## Output

* `sos:CsnArr`: a real `(n, 6)` array of second-order sections.

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
; csnbuttersos.csd
;
; Digital Butterworth filters as second-order sections, as
; scipy.signal.butter with output='sos'. Each row is [b0 b1 b2 a0 a1 a2],
; a0 = 1, and the filter is the cascade of the rows. Sections keep a
; high-order or narrow-band design stable where a single b, a loses it: the
; sixth-order bandpass below has a pole outside the unit circle as b, a.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    ; eighth-order lowpass at 1 kHz: four sections, the sharpest last
    sos:CsnArr = csnbuttersos(8, 1000, 0, sr)
    csnprint(sos)

    ; sixth-order bandpass, 500 to 700 Hz: six stable sections
    iband[] = fillarray(500, 700)
    sosb:CsnArr = csnbuttersos(6, iband, 2, sr)
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

* [csnbutterba](csnbutterba.md)
* [csnzpktosos](csnzpktosos.md)
* [csnbuttap](csnbuttap.md)

## Credits

Pasquale Mainolfi, 2026
