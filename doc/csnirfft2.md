# csnirfft2

## Abstract

Reconstruct a real 2-D array from a spectrum with a one-sided last axis.

## Description

`csnirfft2` is the inverse of [csnrfft2](csnrfft2.md). It expects a full
frequency axis 0 and `cols_nfft / 2 + 1` bins on axis 1, reconstructs the
conjugate half internally and returns real data of shape
`(rows_nfft, cols_nfft)`.

Missing bins are zero-filled and excess bins are ignored. Both transform sizes
must be positive powers of two. Unlike [csnifft2](csnifft2.md), the API output
is real.

## Syntax

```csound
signal:CsnArr = csnirfft2(spectrum:CsnArr, rows_nfft:i, cols_nfft:i)
signal:CsnArr = csnirfft2(spectrum:CsnArr, rows_nfft:i, cols_nfft:i, trig:k)
```

## Arguments

* `spectrum:CsnArr`: a 2-D spectrum with a one-sided last axis.
* `rows_nfft:i`: reconstructed length for axis 0, a positive power of two.
* `cols_nfft:i`: reconstructed length for axis 1, a positive power of two.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes the previous result.

## Output

* `signal:CsnArr`: real array of shape `(rows_nfft, cols_nfft)`.

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
    spectrum:CsnArr = csnrfft2(matrix, 2, 4)
    restored:CsnArr = csnirfft2(spectrum, 2, 4)
    prints("IRFFT2 type = %d\n", csntype(restored))
    csnprint restored
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnrfft2](csnrfft2.md)
* [csnifft2](csnifft2.md)
* [csnirfft](csnirfft.md)

## Credits

Pasquale Mainolfi, 2026
