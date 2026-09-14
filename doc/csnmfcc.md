# csnmfcc

## Abstract

Compute mel-frequency cepstral coefficients of a one-dimensional real signal.

## Description

`csnmfcc` runs the whole analysis chain in one opcode: a short-time Fourier
transform of the signal, a triangular mel filterbank over the power spectrum of
each frame, the natural logarithm of the band energies, and a discrete cosine
transform down the mel axis. It returns a real `(coefficients, frames)` matrix.

The filterbank triangles are evaluated at each bin's own centre frequency rather
than built from rounded bin edges, so a band narrower than the bin spacing still
collects fractional weight from its neighbours instead of collapsing to nothing.
Bands are Slaney-normalized to unit area in frequency, which keeps a wide
high band from outweighing a narrow low one. If a band ends up with no bin
inside it at all, which takes a very small `nfft` against a large band count,
the opcode says so and leaves that band at the log floor.

The cepstral stage is an explicit `coefficients x bands` table built once at
initialization, not a call into the generic DCT: at filterbank sizes a direct
product is the cheaper transform, it needs no scratch, and it places no
arithmetic condition on the band count.

One argument sets both the number of mel bands and the number of coefficients
kept. The frame count follows the STFT rule, `1 + floor((size - nfft) / hop)`,
or one zero-padded frame when the signal is shorter than a frame.

The source must be one-dimensional and real, and `nfft` a power of two. The
k-rate form refuses a source whose length differs from the one it was set up
for, since every buffer in the chain is sized once at initialization.

| window | value |
|--------|-------|
| rectangular | `0` |
| Hann | `1` |
| Hamming | `2` |
| Bartlett | `3` |

| DCT type | value |
|----------|-------|
| DCT-I | `1` |
| DCT-II | `2` |

## Syntax

```csound
coeffs:CsnArr = csnmfcc(source:CsnArr, nfft:i, hop:i, sample_rate:i, ncoeff:i, low_freq:i, high_freq:i, window:i, dct_type:i)
coeffs:CsnArr = csnmfcc(source:CsnArr, nfft:i, hop:i, sample_rate:i, ncoeff:i, low_freq:i, high_freq:i, window:i, dct_type:i, trig:k)
```

## Arguments

* `source:CsnArr`: a one-dimensional real array.
* `nfft:i`: frame and FFT length, a positive power of two.
* `hop:i`: distance between consecutive frame starts, in samples.
* `sample_rate:i`: positive sample rate, used to place the band edges.
* `ncoeff:i`: number of mel bands, and of cepstral coefficients returned.
* `low_freq:i`: lower edge of the filterbank, in hertz.
* `high_freq:i`: upper edge, in hertz; must exceed `low_freq` and not exceed the Nyquist frequency.
* `window:i`: window selector from the table above.
* `dct_type:i`: `1` or `2`, selecting the cepstral transform.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes the previous result.

## Output

* `coeffs:CsnArr`: a real `(ncoeff, frames)` matrix.

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
    ; 128 samples, 32-sample frames hopping by 16: seven frames.
    shape:i[] = fillarray(128)
    source:CsnArr = csnzeros(shape)
    coeffs:CsnArr = csnmfcc(source, 32, 16, 48000, 4, 0, 24000, 2, 2)
    coeff_shape:i[] = csnshape(coeffs)
    prints("MFCC shape = %d coefficients x %d frames\n", coeff_shape[0], coeff_shape[1])
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnmfbank](csnmfbank.md)
* [csnmlogfbank](csnmlogfbank.md)
* [csndcttwo1d](csndcttwo1d.md)
* [csnstft](csnstft.md)

## Credits

Pasquale Mainolfi, 2026
