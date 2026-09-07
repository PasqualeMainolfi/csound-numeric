# csnifft

## Abstract

Compute the inverse complex discrete Fourier transform along one axis.

## Description

`csnifft` computes an `nfft`-point inverse FFT and returns a complex array. It
is normalized so that `csnifft(csnfft(x, nfft), nfft)` reconstructs the samples
represented by the transform, apart from floating-point roundoff.

The selected source axis is zero-padded or truncated to `nfft`; the selected
output axis always has `nfft` elements. The inverse does not discard its
imaginary lane. For a one-sided spectrum produced by [csnrfft](csnrfft.md), use
[csnirfft](csnirfft.md) instead.

## Syntax

```csound
signal:CsnArr = csnifft(spectrum:CsnArr, nfft:i)
signal:CsnArr = csnifft(spectrum:CsnArr, nfft:i, axis:i)
signal:CsnArr = csnifft(spectrum:CsnArr, nfft:i, axis:i, trig:k)
```

## Arguments

* `spectrum:CsnArr`: normally a full complex spectrum; real arrays are also accepted.
* `nfft:i`: inverse transform length, a positive power of two.
* `axis:i` (optional, default `-1`): transform axis; `-1` means the last axis.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes the previous result.

## Output

* `signal:CsnArr`: complex array with `nfft` elements on the transform axis.

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
    source:CsnArr = csnfromarray(array(1, 2, 3, 4))
    spectrum:CsnArr = csnfft(source, 4)
    restored:CsnArr = csnifft(spectrum, 4)
    re:CsnArr = csnreal(restored)
    im:CsnArr = csnimag(restored)
    re_out:i[] = csntoarray(re)
    prints("IFFT real lane: %g %g %g %g\n", re_out[0], re_out[1], re_out[2], re_out[3])
    prints("maximum imaginary residue: %g\n", csnmax(csnabs(im)))
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnfft](csnfft.md)
* [csnirfft](csnirfft.md)
* [csnifftshift](csnifftshift.md)

## Credits

Pasquale Mainolfi, 2026
