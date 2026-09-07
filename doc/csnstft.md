# csnstft

## Abstract

Compute the short-time Fourier transform of a one-dimensional array.

## Description

`csnstft` splits a 1-D signal into overlapping frames, multiplies each frame by
a window and transforms it. It returns frequency coordinates, frame-centre time
coordinates and a complex matrix whose shape is `(bins, frames)`.

For real input it uses a real FFT and returns `nfft / 2 + 1` bins. For complex
input it returns all `nfft` bins. If the input is shorter than `nfft`, one
zero-padded frame is emitted. Otherwise the frame count is
`1 + floor((size - nfft) / hop)`; an incomplete tail after the last full frame
is not emitted.

The frequency values are in hertz. Time values are in seconds and identify the
centre of each analysis window: `(frame * hop + nfft / 2) / sample_rate`.
`nfft`, `hop`, `sample_rate` and the window choice are fixed at initialization.

| window | value |
|--------|-------|
| rectangular | `0` |
| Hann | `1` |
| Hamming | `2` |
| Bartlett | `3` |

## Syntax

```csound
freqs:CsnArr, times:CsnArr, frames:CsnArr = csnstft(source:CsnArr, nfft:i, hop:i, sample_rate:i)
freqs:CsnArr, times:CsnArr, frames:CsnArr = csnstft(source:CsnArr, nfft:i, hop:i, sample_rate:i, window:i)
freqs:CsnArr, times:CsnArr, frames:CsnArr = csnstft(source:CsnArr, nfft:i, hop:i, sample_rate:i, window:i, trig:k)
```

## Arguments

* `source:CsnArr`: a one-dimensional real or complex array.
* `nfft:i`: frame and FFT length, a positive power of two.
* `hop:i`: distance between consecutive frame starts, in samples.
* `sample_rate:i`: positive sample rate used for the coordinate outputs.
* `window:i` (optional, default `0`): window selector from the table above.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes all previous outputs.

## Output

* `freqs:CsnArr`: real 1-D frequency-bin coordinates in hertz.
* `times:CsnArr`: real 1-D frame-centre coordinates in seconds.
* `frames:CsnArr`: complex `(bins, frames)` STFT matrix.

## Execution Time

* Init
* Performance (k-rate)

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    ; nfft=4, hop=2, sample rate=8 Hz, rectangular window.
    source:CsnArr = csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8))
    freqs:CsnArr, times:CsnArr, frames:CsnArr = csnstft(source, 4, 2, 8, 0)
    frame_shape:i[] = csnshape(frames)
    prints("STFT shape = %d bins x %d frames\n", frame_shape[0], frame_shape[1])
    csnprint freqs
    csnprint times
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnistft](csnistft.md)
* [csnrfft](csnrfft.md)
* [csnfftfreq](csnfftfreq.md)
* [csnhanning](csnhanning.md)

## Credits

Pasquale Mainolfi, 2026
