# csnifft2

## Abstract

Compute a two-dimensional inverse complex FFT.

## Description

`csnifft2` computes the inverse transform along both axes of a 2-D spectrum and
returns complex samples. The result is normalized for a direct
`csnfft2`/`csnifft2` round trip.

The spectrum is zero-padded or truncated as needed and the output shape is
always `(rows_nfft, cols_nfft)`. Both sizes must be positive powers of two.
The imaginary lane is retained; use [csnirfft2](csnirfft2.md) for a spectrum
whose last axis is the one-sided result of [csnrfft2](csnrfft2.md).

## Syntax

```csound
signal:CsnArr = csnifft2(spectrum:CsnArr, rows_nfft:i, cols_nfft:i)
signal:CsnArr = csnifft2(spectrum:CsnArr, rows_nfft:i, cols_nfft:i, trig:k)
```

## Arguments

* `spectrum:CsnArr`: a 2-D full spectrum, normally complex.
* `rows_nfft:i`: inverse length for axis 0, a positive power of two.
* `cols_nfft:i`: inverse length for axis 1, a positive power of two.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes the previous result.

## Output

* `signal:CsnArr`: complex array of shape `(rows_nfft, cols_nfft)`.

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
    shape:i[] = fillarray(2, 4)
    matrix:CsnArr = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8)), shape)
    spectrum:CsnArr = csnfft2(matrix, 2, 4)
    restored:CsnArr = csnifft2(spectrum, 2, 4)
    realpart:CsnArr = csnreal(restored)
    prints("IFFT2 type = %d\n", csntype(restored))
    csnprint realpart
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnfft2](csnfft2.md)
* [csnirfft2](csnirfft2.md)
* [csnifft](csnifft.md)

## Credits

Pasquale Mainolfi, 2026
