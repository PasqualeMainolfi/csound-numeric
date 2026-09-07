# csnrfft

## Abstract

Compute the non-negative-frequency half of the FFT of a real array.

## Description

`csnrfft` computes an `nfft`-point FFT of real input and returns the one-sided
complex spectrum from DC through Nyquist. Its length along `axis` is
`floor(nfft / 2) + 1`; for the required power-of-two sizes this is
`nfft / 2 + 1`.

The selected source axis is zero-padded or truncated to `nfft` before the
transform. All other dimensions are preserved. Complex input is rejected.
Use [csnirfft](csnirfft.md) for the inverse.

## Syntax

```csound
spectrum:CsnArr = csnrfft(source:CsnArr, nfft:i)
spectrum:CsnArr = csnrfft(source:CsnArr, nfft:i, axis:i)
spectrum:CsnArr = csnrfft(source:CsnArr, nfft:i, axis:i, trig:k)
```

## Arguments

* `source:CsnArr`: real input array.
* `nfft:i`: transform length, a positive power of two.
* `axis:i` (optional, default `-1`): transform axis; `-1` means the last axis.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes the previous result.

## Output

* `spectrum:CsnArr`: complex array with `nfft / 2 + 1` bins on the transform axis.

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
    ; For eight real samples only DC through Nyquist are returned: five bins.
    source:CsnArr = csnfromarray(array(1, 0, 0, 0, 0, 0, 0, 0))
    spectrum:CsnArr = csnrfft(source, 8)
    prints("real FFT: type = %d, bins = %d\n", csntype(spectrum), csnsize(spectrum))
    csnprint spectrum
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnirfft](csnirfft.md)
* [csnfft](csnfft.md)
* [csnrfftfreq](csnrfftfreq.md)

## Credits

Pasquale Mainolfi, 2026
