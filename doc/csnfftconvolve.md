# csnfftconvolve

## Abstract

N-dimensional convolution with a kernel of the same rank, computed through Fourier transforms.

## Description

`csnfftconvolve` is [csnconvolve](csnconvolve.md) computed in the spectrum. An
N-D transform is separable, so it runs one pass per axis: both operands are
zero-padded onto a common grid - each axis to its own power of two at or above
`x[i] + h[i] - 1` - transformed axis by axis, multiplied, and transformed back.
It is `scipy.signal.fftconvolve`, and for a 2-D pair the FFT method of
`scipy.signal.convolve2d`.

In two dimensions the direct form costs the product of four extents, which is
why a large kernel over a large field is the case this opcode exists for: a
9x9 blur over a 32x32 field is 81 multiply-adds per output cell directly, and
a fixed number of transforms here.

The results match the direct form to within floating-point rounding, not bit
for bit.

Everything else follows [csnconvolve](csnconvolve.md): both arrays must have
the same rank, no axis of the kernel may be longer than the matching axis of
the source, the kernel must be non-empty, and `edges` selects `0` FULL, `1`
SAME, `2` VALID with the same extents. Outside the source counts as zero. Both
real and complex arrays are accepted.

At k-rate the padded grid follows the operands, so a source or a kernel that
changes shape gets a transform sized for it.

[csnfftcorrelate](csnfftcorrelate.md) is the correlation counterpart, and
[csnfftconvolve1d](csnfftconvolve1d.md) applies a vector along one axis and
leaves the others alone.

## Syntax

```csound
handle:CsnArr = csnfftconvolve(x:CsnArr, h:CsnArr)
handle:CsnArr = csnfftconvolve(x:CsnArr, h:CsnArr, edges:i)
handle:CsnArr = csnfftconvolve(x:CsnArr, h:CsnArr, edges:i, trig:k)
```

## Arguments

* `x:CsnArr`: the array to convolve.
* `h:CsnArr`: the kernel; same rank as `x`, no axis longer than the matching one, at least one element.
* `edges:i` (optional, default `0`): `0` FULL, `1` SAME, `2` VALID.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the convolution.

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

; -----------------------------------------------------------------------------
; csnfftconvolve.csd
;
; The N-D convolution through transforms: one pass per axis, a product in the
; spectrum, one inverse pass per axis. Same kernel, same edge modes and same
; answers as csnconvolve, and the same shape rules.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]      = fillarray(3, 3)
    kshape:i[]     = fillarray(2, 2)
    source:CsnArr  = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8, 9)), shape)
    kernel:CsnArr  = csnreshape(csnfromarray(array(1, 2, 3, 4)), kshape)

    full:CsnArr    = csnfftconvolve(source, kernel, 0)
    csnprint full

    valid:CsnArr   = csnfftconvolve(source, kernel, 2)
    csnprint valid

    ; a 32x32 field through a 9x9 blur: every axis is padded to its own power
    ; of two, so the two extents are transformed at different lengths
    field_shape:i[] = fillarray(32, 32)
    blur_shape:i[]  = fillarray(9, 9)
    field:CsnArr    = csnreshape(csnsin(csnarange(0, 1024, 1)), field_shape)
    blur:CsnArr     = csnfull(blur_shape, 1 / 81)

    smooth:CsnArr   = csnfftconvolve(field, blur, 1)
    smooth_shape:i[] = csnshape(smooth)
    prints("SAME keeps the field shape: %d x %d\n", smooth_shape[0], smooth_shape[1])

    direct:CsnArr   = csnconvolve(field, blur, 1)
    worst:i         = csnmax(csnabs(csnsubtract(direct, smooth)))
    prints("largest difference from csnconvolve: %.2g\n", worst)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnconvolve](csnconvolve.md)
* [csnfftcorrelate](csnfftcorrelate.md)
* [csnfftconvolve1d](csnfftconvolve1d.md)
* [csnrfft2](csnrfft2.md)

## Credits

Pasquale Mainolfi, 2026
