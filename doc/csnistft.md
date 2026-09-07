# csnistft

## Abstract

Reconstruct a one-dimensional signal from an STFT matrix.

## Description

`csnistft` inverse-transforms and overlap-adds a complex `(bins, frames)` matrix.
It applies the selected synthesis window and divides by the accumulated squared
window, so matching [csnstft](csnstft.md) parameters reconstruct the represented
samples apart from floating-point roundoff.

An input with `nfft / 2 + 1` rows is interpreted as a one-sided real spectrum
and produces a real signal. An input with `nfft` rows is interpreted as a full
spectrum and produces a complex signal. No other row count is accepted. The
output length is `nfft + (frames - 1) * hop`; at least one frame is required.

The first result is a sample-time vector in seconds, beginning at zero. The
second is the reconstructed signal. Parameters and window codes are the same as
for `csnstft` and are fixed at initialization.

| window | value |
|--------|-------|
| rectangular | `0` |
| Hann | `1` |
| Hamming | `2` |
| Bartlett | `3` |

## Syntax

```csound
times:CsnArr, signal:CsnArr = csnistft(frames:CsnArr, nfft:i, hop:i, sample_rate:i)
times:CsnArr, signal:CsnArr = csnistft(frames:CsnArr, nfft:i, hop:i, sample_rate:i, window:i)
times:CsnArr, signal:CsnArr = csnistft(frames:CsnArr, nfft:i, hop:i, sample_rate:i, window:i, trig:k)
```

## Arguments

* `frames:CsnArr`: complex 2-D STFT matrix shaped `(bins, frames)`.
* `nfft:i`: inverse FFT and window length, a positive power of two.
* `hop:i`: distance between consecutive frame starts, in samples.
* `sample_rate:i`: positive sample rate used for the time coordinates.
* `window:i` (optional, default `0`): window selector from the table above.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes both previous outputs.

## Output

* `times:CsnArr`: real sample-time coordinates in seconds.
* `signal:CsnArr`: real output for a one-sided spectrum, complex output for a full spectrum.

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
    source:CsnArr = csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8))
    freqs:CsnArr, frame_times:CsnArr, frames:CsnArr = csnstft(source, 4, 2, 8, 0)
    sample_times:CsnArr, restored:CsnArr = csnistft(frames, 4, 2, 8, 0)
    out:i[] = csntoarray(restored)
    prints("ISTFT type = %d, samples = %d\n", csntype(restored), csnsize(restored))
    prints("first/last = %g %g\n", out[0], out[7])
    csnprint sample_times
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnstft](csnstft.md)
* [csnirfft](csnirfft.md)
* [csnstream](csnstream.md)

## Credits

Pasquale Mainolfi, 2026
