# csnfft2

## Abstract

Compute a two-dimensional complex FFT.

## Description

`csnfft2` transforms a 2-D real or complex array along both axes and returns a
full complex spectrum. `rows_nfft` is the transform length for axis 0 and
`cols_nfft` for axis 1. Both must be positive powers of two.

Each dimension is independently zero-padded or truncated to its requested
length, so the output shape is always `(rows_nfft, cols_nfft)`. The transform
sizes are i-rate values and remain fixed for the lifetime of the opcode
instance, including its k-rate form.

## Syntax

```csound
spectrum:CsnArr = csnfft2(source:CsnArr, rows_nfft:i, cols_nfft:i)
spectrum:CsnArr = csnfft2(source:CsnArr, rows_nfft:i, cols_nfft:i, trig:k)
```

## Arguments

* `source:CsnArr`: a 2-D real or complex array.
* `rows_nfft:i`: transform length for rows (axis 0), a positive power of two.
* `cols_nfft:i`: transform length for columns (axis 1), a positive power of two.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes the previous result.

## Output

* `spectrum:CsnArr`: complex array of shape `(rows_nfft, cols_nfft)`.

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
    spectrum:CsnArr = csnfft2(matrix, 2, 4)
    prints("FFT2: type = %d, elements = %d\n", csntype(spectrum), csnsize(spectrum))
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

* [csnifft2](csnifft2.md)
* [csnrfft2](csnrfft2.md)
* [csnfft](csnfft.md)

## Credits

Pasquale Mainolfi, 2026
