# csnfft

## Abstract

Compute a complex discrete Fourier transform along one axis.

## Description

`csnfft` computes an `nfft`-point forward FFT. Real input is accepted and
treated as complex data with a zero imaginary part; the output is always
complex and contains the complete spectrum.

The transform is applied independently to every slice along `axis`. The other
dimensions are preserved and the selected dimension becomes `nfft`. A shorter
source axis is zero-padded; a longer one is truncated. `nfft` must be a positive
power of two and is fixed when the opcode instance initializes.

Use [csnrfft](csnrfft.md) for a real source when the redundant negative-frequency
half is not needed.

## Syntax

```csound
spectrum:CsnArr = csnfft(source:CsnArr, nfft:i)
spectrum:CsnArr = csnfft(source:CsnArr, nfft:i, axis:i)
spectrum:CsnArr = csnfft(source:CsnArr, nfft:i, axis:i, trig:k)
```

## Arguments

* `source:CsnArr`: real or complex input array.
* `nfft:i`: transform length, a positive power of two.
* `axis:i` (optional, default `-1`): transform axis; `-1` means the last axis.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes the previous result.

## Output

* `spectrum:CsnArr`: complex array, with the selected axis replaced by `nfft`.

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
    ; A unit impulse has a flat complex spectrum.
    source:CsnArr = csnfromarray(array(1, 0, 0, 0, 0, 0, 0, 0))
    spectrum:CsnArr = csnfft(source, 8)
    prints("full FFT: type = %d, bins = %d\n", csntype(spectrum), csnsize(spectrum))
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

* [csnifft](csnifft.md)
* [csnrfft](csnrfft.md)
* [csnfftfreq](csnfftfreq.md)
* [csnfftshift](csnfftshift.md)

## Credits

Pasquale Mainolfi, 2026
