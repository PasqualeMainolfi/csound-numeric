# csnirfft

## Abstract

Reconstruct a real array from a one-sided complex spectrum.

## Description

`csnirfft` is the inverse of [csnrfft](csnrfft.md). It reads the DC-through-
Nyquist half-spectrum, reconstructs the conjugate half internally and returns
`nfft` real samples along the selected axis.

The expected spectrum length is `nfft / 2 + 1`. If fewer bins are present they
are zero-filled; extra bins are ignored. The result is normalized for a direct
`csnrfft`/`csnirfft` round trip. Unlike [csnifft](csnifft.md), its API output is
real, not complex.

## Syntax

```csound
signal:CsnArr = csnirfft(spectrum:CsnArr, nfft:i)
signal:CsnArr = csnirfft(spectrum:CsnArr, nfft:i, axis:i)
signal:CsnArr = csnirfft(spectrum:CsnArr, nfft:i, axis:i, trig:k)
```

## Arguments

* `spectrum:CsnArr`: the one-sided spectrum, normally complex.
* `nfft:i`: reconstructed length, a positive power of two.
* `axis:i` (optional, default `-1`): transform axis; `-1` means the last axis.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes the previous result.

## Output

* `signal:CsnArr`: real array with `nfft` elements on the transform axis.

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
    spectrum:CsnArr = csnrfft(source, 8)
    restored:CsnArr = csnirfft(spectrum, 8)
    out:i[] = csntoarray(restored)
    prints("IRFFT type = %d, samples = %d\n", csntype(restored), csnsize(restored))
    prints("first/last = %g %g\n", out[0], out[7])
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnrfft](csnrfft.md)
* [csnifft](csnifft.md)
* [csnrfftfreq](csnrfftfreq.md)

## Credits

Pasquale Mainolfi, 2026
