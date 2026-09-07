# csnfftfreq

## Abstract

Return the frequency coordinates for a full FFT.

## Description

`csnfftfreq` creates the bin coordinates corresponding to an `n`-point full
FFT. Given sample spacing `d`, the bin spacing is `1 / (n*d)`. Positive bins
come first, followed by the negative-frequency bins in native FFT order.

For even `n` the Nyquist bin is represented as negative. For example,
`csnfftfreq(8, 1/8)` returns `0, 1, 2, 3, -4, -3, -2, -1` Hz. Apply
[csnfftshift](csnfftshift.md) when a negative-to-positive centred ordering is
more convenient.

## Syntax

```csound
freqs:CsnArr = csnfftfreq(n:i, d:i)
freqs:CsnArr = csnfftfreq(n:k, d:k)
freqs:CsnArr = csnfftfreq(n:k, d:k, trig:k)
```

## Arguments

* `n:i / n:k`: positive integer FFT length; unlike the transforms, it need not be a power of two.
* `d:i / d:k`: positive sample spacing, in seconds for a result in hertz.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes the previous result.

## Output

* `freqs:CsnArr`: real 1-D array of `n` frequency coordinates.

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
    ; Eight samples spaced 1/8 second apart: bin spacing is 1 Hz.
    freqs:CsnArr = csnfftfreq(8, 0.125)
    out:i[] = csntoarray(freqs)
    prints("fftfreq = %g %g %g %g %g %g %g %g\n", out[0], out[1], out[2], out[3], out[4], out[5], out[6], out[7])
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnrfftfreq](csnrfftfreq.md)
* [csnfft](csnfft.md)
* [csnfftshift](csnfftshift.md)

## Credits

Pasquale Mainolfi, 2026
