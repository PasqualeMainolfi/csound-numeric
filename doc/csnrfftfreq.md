# csnrfftfreq

## Abstract

Return the non-negative frequency coordinates for a real FFT.

## Description

`csnrfftfreq` returns the coordinates for the one-sided spectrum produced by
[csnrfft](csnrfft.md). The result has `floor(n / 2) + 1` entries, starting at
zero. Given sample spacing `d`, adjacent bins are `1 / (n*d)` apart. For even
`n`, the last entry is the positive Nyquist frequency.

For example, `csnrfftfreq(8, 1/8)` returns `0, 1, 2, 3, 4` Hz.

## Syntax

```csound
freqs:CsnArr = csnrfftfreq(n:i, d:i)
freqs:CsnArr = csnrfftfreq(n:k, d:k)
freqs:CsnArr = csnrfftfreq(n:k, d:k, trig:k)
```

## Arguments

* `n:i / n:k`: positive integer FFT length; it need not be a power of two.
* `d:i / d:k`: positive sample spacing, in seconds for a result in hertz.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes the previous result.

## Output

* `freqs:CsnArr`: real 1-D array of `floor(n / 2) + 1` coordinates.

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
    freqs:CsnArr = csnrfftfreq(8, 0.125)
    out:i[] = csntoarray(freqs)
    prints("rfftfreq = %g %g %g %g %g\n", out[0], out[1], out[2], out[3], out[4])
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnfftfreq](csnfftfreq.md)
* [csnrfft](csnrfft.md)
* [csnstft](csnstft.md)

## Credits

Pasquale Mainolfi, 2026
