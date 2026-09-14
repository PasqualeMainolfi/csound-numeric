# csnmlogfbank

## Abstract

Build a triangular mel filterbank matrix in logarithmic scale.

## Description

`csnmlogfbank` returns the filterbank itself rather than applying it: a real
`(bands, nfft / 2 + 1)` matrix with one row per mel band over the bins of a
one-sided spectrum, the shape `librosa.filters.mel` returns. Multiplying a power
spectrum by this matrix gives the band energies.

One row per band is not a convenience: adjacent bands always overlap, band `i`
spanning the edges `i` and `i+2` while band `i+1` spans `i+1` and `i+3`, so they
cannot share a row without erasing each other.

Every bin of every row is the natural logarithm of the linear weight, with a
floor of `1e-12`, so the bins no band reaches read as that floor rather than
as zero: the whole row is in one scale.

Triangles are evaluated at each bin's own centre frequency rather than built
from rounded bin edges, so a band narrower than the bin spacing still collects
fractional weight from its neighbours. Without normalization the rows form a
partition of unity between the first and last band centre; with it each band is
scaled to unit area in frequency, which keeps a wide high band from outweighing
a narrow low one.

`nfft` must be a power of two, matching what the transforms accept: a bank built
for any other length could not be applied to a spectrogram this library
produces.

## Syntax

```csound
bank:CsnArr = csnmlogfbank(nfft:i, bands:i, low_freq:i, high_freq:i, sample_rate:i, slaney_norm:i)
```

## Arguments

* `nfft:i`: FFT length the bank is built for, a positive power of two.
* `bands:i`: number of mel bands, one per row.
* `low_freq:i`: lower edge of the filterbank, in hertz.
* `high_freq:i`: upper edge, in hertz; must exceed `low_freq` and not exceed the Nyquist frequency.
* `sample_rate:i`: positive sample rate, used to place the band edges.
* `slaney_norm:i`: `1` to scale each band to unit area in frequency, `0` to leave the triangles at unit peak.

## Output

* `bank:CsnArr`: a real `(bands, nfft / 2 + 1)` matrix.

## Execution Time

* Init

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
    ; Four bands over the 17 bins of a 32-point one-sided spectrum.
    bank:CsnArr = csnmlogfbank(32, 4, 0, 24000, 48000, 0)
    bank_shape:i[] = csnshape(bank)
    prints("filterbank shape = %d bands x %d bins\n", bank_shape[0], bank_shape[1])
    csnprint bank
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
* [csnmfcc](csnmfcc.md)
* [csnstft](csnstft.md)

## Credits

Pasquale Mainolfi, 2026
