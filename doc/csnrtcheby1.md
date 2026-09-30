# csnrtcheby1

## Abstract

A real-time digital Chebyshev type I lowpass, highpass, bandpass or bandstop filter on audio, with a k-rate cutoff.

## Description

`csnrtcheby1` filters an audio signal with a digital Chebyshev type I filter whose
frequencies may change every control period.
The filter is equiripple in the passband, `rp` dB down at the cutoff.

The analog prototype is the Chebyshev type I prototype, designed once at init,
as [csncheby1sos](csncheby1sos.md) designs it; the order and the type are fixed for
the note. At every control period the prototype is moved to the current
frequencies, pre-warped with `2 fs tan(pi f / fs)`, mapped by the bilinear
transform and turned into second-order sections, which then filter the block.
Nothing is allocated at performance time, so the opcode is safe on a
real-time path.

Each pole pair of the prototype, with its zero pair when it has one, stays one
section (two for a band) whatever the cutoff, so the section state carries
over from one control period to the next and a moving cutoff does not reset
the filter. Each section is normalised to unit gain where the prototype's DC
gain lands, DC for a lowpass and a bandstop, Nyquist for a highpass, the
centre for a bandpass, and the first one carries that gain. With a constant
cutoff the output matches [csncheby1sos](csncheby1sos.md) followed by
[csnsosfilter](csnsosfilter.md) to within rounding; the sections are paired
and scaled differently, not the filter.

A band is given by its centre and its width in Hz: the edges are
`fc - bw / 2` and `fc + bw / 2`, the two cutoffs [csncheby1sos](csncheby1sos.md)
takes as an array. Both may change at k-rate.

The order must be an integer of at least one, and a single cutoff is only for
a lowpass or a highpass, a centre and a width only for a bandpass or a
bandstop. A cutoff outside `(0, fs / 2)`, or a band whose edges leave it or
whose width is not positive, is a performance error.

## Syntax

```csound
aout = csnrtcheby1(ain, order:i, fc:k, rp:i, type:i, fs:i)
aout = csnrtcheby1(ain, order:i, fc:k, bw:k, rp:i, type:i, fs:i)
```

## Arguments

* `ain`: the audio signal to filter.
* `order:i`: the filter order, an integer greater or equal to one.
* `fc:k`: the cutoff in Hz for a lowpass or a highpass; the band centre in Hz for a bandpass or a bandstop.
* `bw:k`: the band width in Hz, greater than zero, for a bandpass or a bandstop.
* `rp:i`: the passband ripple in dB, finite and greater than zero.
* `type:i`: `0` lowpass, `1` highpass with a single cutoff; `2` bandpass, `3` bandstop with a centre and a width.
* `fs:i`: the sampling rate in Hz, a positive integer; normally `sr`.

## Output

* `aout`: the filtered audio signal.

## Execution Time

* Init
* Performance (audio)

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnrtcheby1.csd
;
; A digital Chebyshev type I filter on audio whose cutoff, or band centre and
; width, follow a k-rate signal. The prototype is designed once at init; each
; control period moves it to the current frequencies and filters the block
; through the second-order sections, keeping the state across the change.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    ; lowpass sweeping from 200 Hz to 5 kHz and back
    anoise = noise(0.3, 0)
    kfc = expseg(200, p3 / 2, 5000, p3 / 2, 200)
    afilt = csnrtcheby1(anoise, 5, kfc, 1, 0, sr)
    out afilt
endin

instr 2
    ; bandpass whose centre glides up while the band narrows
    anoise = noise(0.3, 0)
    kcentre = expseg(300, p3, 3000)
    kwidth = linseg(400, p3, 100)
    afilt = csnrtcheby1(anoise, 4, kcentre, kwidth, 1, 2, sr)
    out afilt
endin

</CsInstruments>
<CsScore>
i 1 0 2
i 2 2 2
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csncheby1sos](csncheby1sos.md)
* [csnsosfilter](csnsosfilter.md)
* [csnzsosfilter](csnzsosfilter.md)
* [csnrtbutter](csnrtbutter.md)
* [csnrtcheby2](csnrtcheby2.md)
* [csnrtellip](csnrtellip.md)

## Credits

Pasquale Mainolfi, 2026
