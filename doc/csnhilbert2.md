# csnhilbert2

## Abstract

Compute the two-dimensional analytic signal of a real matrix.

## Description

`csnhilbert2` is the two-dimensional counterpart of `csnhilbert1d`, matching
`scipy.signal.hilbert2`. The mask is the outer product of the two
one-dimensional masks, one per axis, applied to the two-dimensional spectrum:

```
a = ifft2(fft2(x) * (h_rows (x) h_cols))
```

Being separable, the mask is never built as a matrix: scaling by `h_rows[i]`
along the rows and then by `h_cols[j]` along the columns is the same operation
and needs only the two vectors.

There is no real counterpart to this opcode the way `csnhilbert1dr` answers
`csnhilbert1d`. In one dimension the real part of the analytic signal is the
source exactly, which is what makes the imaginary part alone meaningful. In two
dimensions that no longer holds: the product mask is not such that a frequency
and its mirror sum to a constant, so the real part is not the source and the
rotation trick behind the real form does not apply.

> This is not Csound's `hilbert` opcode. That one is a twelve-pole allpass
> network running on an audio signal, sample by sample, and it returns two
> signals about ninety degrees apart over a usable band. This works on a whole
> array at once, is exact inside the block rather than approximate over a band,
> and carries no state between calls.

Both extents are the transform lengths and neither is padded, for the same
reason as in one dimension. Both must be even and at least two. The input must
be real and two-dimensional, and the k-rate form refuses a source whose shape
differs from the one it was set up for.

## Syntax

```csound
analytic:CsnArr = csnhilbert2(source:CsnArr)
analytic:CsnArr = csnhilbert2(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: a real two-dimensional array, both extents even and at least two.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes the previous result.

## Output

* `analytic:CsnArr`: a complex array shaped like the source.

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
    values:i[] = fillarray(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                           13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24)
    shape:i[] = fillarray(4, 6)
    source:CsnArr = csnreshape(csnfromarray(values), shape)
    analytic:CsnArr = csnhilbert2(source)
    csnprint csnimag(analytic)
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnhilbert1d](csnhilbert1d.md)
* [csnhilbert1dr](csnhilbert1dr.md)
* [csnfft2](csnfft2.md)

## Credits

Pasquale Mainolfi, 2026
