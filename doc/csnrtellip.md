# csnrtellip

## Abstract

A real-time digital elliptic lowpass, highpass, bandpass or bandstop filter on audio, with a k-rate cutoff.

## Description

`csnrtellip` filters an audio signal with a digital elliptic filter whose
frequencies may change every control period.
The filter is equiripple in both bands, `rp` dB down at the cutoff.

The analog prototype is the elliptic prototype, designed once at init,
as [csnellipsos](csnellipsos.md) designs it; the order and the type are fixed for
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
cutoff the output matches [csnellipsos](csnellipsos.md) followed by
[csnsosfilter](csnsosfilter.md) to within rounding; the sections are paired
and scaled differently, not the filter.

A band is given by its centre and its width in Hz: the edges are
`fc - bw / 2` and `fc + bw / 2`, the two cutoffs [csnellipsos](csnellipsos.md)
takes as an array. Both may change at k-rate.

The order must be an integer of at least one, and a single cutoff is only for
a lowpass or a highpass, a centre and a width only for a bandpass or a
bandstop. A cutoff outside `(0, fs / 2)`, or a band whose edges leave it or
whose width is not positive, is a performance error.

## Syntax

```csound
aout = csnrtellip(ain, order:i, fc:k, rp:i, rs:i, type:i, fs:i)
aout = csnrtellip(ain, order:i, fc:k, bw:k, rp:i, rs:i, type:i, fs:i)
```

## Arguments

* `ain`: the audio signal to filter.
* `order:i`: the filter order, an integer greater or equal to one.
* `fc:k`: the cutoff in Hz for a lowpass or a highpass; the band centre in Hz for a bandpass or a bandstop.
* `bw:k`: the band width in Hz, greater than zero, for a bandpass or a bandstop.
* `rp:i`: the passband ripple in dB, finite and greater than zero.
* `rs:i`: the minimum stopband attenuation in dB, finite and greater than `rp`.
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
; csnrtellip.csd
;
; A digital elliptic filter on audio whose cutoff, or band centre and
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
    afilt = csnrtellip(anoise, 4, kfc, 1, 60, 0, sr)
    out afilt
endin

instr 2
    ; bandpass whose centre glides up while the band narrows
    anoise = noise(0.3, 0)
    kcentre = expseg(300, p3, 3000)
    kwidth = linseg(400, p3, 100)
    afilt = csnrtellip(anoise, 4, kcentre, kwidth, 1, 60, 2, sr)
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

* [csnellipsos](csnellipsos.md)
* [csnsosfilter](csnsosfilter.md)
* [csnzsosfilter](csnzsosfilter.md)
* [csnrtbutter](csnrtbutter.md)
* [csnrtcheby1](csnrtcheby1.md)
* [csnrtcheby2](csnrtcheby2.md)

## Credits

Pasquale Mainolfi, 2026
