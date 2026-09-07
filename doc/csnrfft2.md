# csnrfft2

## Abstract

Compute a two-dimensional FFT of real input with a one-sided last axis.

## Description

`csnrfft2` transforms a 2-D real array. Axis 0 retains its complete complex
spectrum, while axis 1 stores only the non-negative-frequency bins. The output
shape is therefore `(rows_nfft, cols_nfft / 2 + 1)`.

Both requested lengths must be positive powers of two. Source dimensions are
zero-padded or truncated to those lengths before transformation. Complex source
arrays are rejected. Use [csnirfft2](csnirfft2.md) to reconstruct real data.

## Syntax

```csound
spectrum:CsnArr = csnrfft2(source:CsnArr, rows_nfft:i, cols_nfft:i)
spectrum:CsnArr = csnrfft2(source:CsnArr, rows_nfft:i, cols_nfft:i, trig:k)
```

## Arguments

* `source:CsnArr`: a 2-D real array.
* `rows_nfft:i`: transform length for axis 0, a positive power of two.
* `cols_nfft:i`: transform length for axis 1, a positive power of two.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes the previous result.

## Output

* `spectrum:CsnArr`: complex array of shape `(rows_nfft, cols_nfft / 2 + 1)`.

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
    matrix:CsnArr = csnreshape(csnfromarray(array(1, 0, 0, 0, 0, 0, 0, 0)), shape)
    spectrum:CsnArr = csnrfft2(matrix, 2, 4)
    outshape:i[] = csnshape(spectrum)
    prints("RFFT2 shape = %d x %d, type = %d\n", outshape[0], outshape[1], csntype(spectrum))
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

* [csnirfft2](csnirfft2.md)
* [csnfft2](csnfft2.md)
* [csnrfft](csnrfft.md)

## Credits

Pasquale Mainolfi, 2026
