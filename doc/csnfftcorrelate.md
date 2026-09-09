# csnfftcorrelate

## Abstract

N-dimensional cross-correlation with a kernel of the same rank, computed through Fourier transforms.

## Description

`csnfftcorrelate` is [csncorrelate](csncorrelate.md) computed in the spectrum:
both operands are zero-padded onto a common grid, transformed one axis at a
time, multiplied with the kernel's spectrum conjugated, and transformed back.
Conjugating in the spectrum is what flipping the kernel on every axis does in
the time domain. It is `scipy.signal.correlate(..., method='fft')`.

Two-dimensional template matching is the usual use, and VALID is the mode for
it: every score comes from a full overlap, so the coordinates of the largest -
from [csnargmax](csnargmax.md) - are where the pattern sits. The larger the
patch, the more this form has over the direct one; the answers agree to within
floating-point rounding.

Everything else follows [csncorrelate](csncorrelate.md): both arrays must have
the same rank, no axis of the kernel may be longer than the matching axis of
the source, the kernel must be non-empty, and `edges` selects FULL, SAME or
VALID with the same extents. Both real and complex arrays are accepted.

At k-rate the padded grid follows the operands rather than staying at whatever
the init pass needed.

[csnfftcorrelate1d](csnfftcorrelate1d.md) applies a vector along one axis and
leaves the others alone.

## Syntax

```csound
handle:CsnArr = csnfftcorrelate(x:CsnArr, h:CsnArr)
handle:CsnArr = csnfftcorrelate(x:CsnArr, h:CsnArr, edges:i)
handle:CsnArr = csnfftcorrelate(x:CsnArr, h:CsnArr, edges:i, trig:k)
```

## Arguments

* `x:CsnArr`: the array to correlate.
* `h:CsnArr`: the kernel; same rank as `x`, no axis longer than the matching one, at least one element.
* `edges:i` (optional, default `0`): `0` FULL, `1` SAME, `2` VALID.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the cross-correlation.

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
; csnfftcorrelate.csd
;
; Template matching in two dimensions, through transforms. The kernel is
; conjugated in the spectrum rather than flipped on every axis, and the answer
; is the one csncorrelate gives.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]     = fillarray(4, 4)
    kshape:i[]    = fillarray(2, 2)

    ; a 4x4 field, empty except for a 2x2 patch at rows 1-2, columns 1-2
    field:CsnArr  = csnreshape(csnfromarray(array(0, 0, 0, 0,
                                                  0, 1, 1, 0,
                                                  0, 1, 1, 0,
                                                  0, 0, 0, 0)), shape)
    patch:CsnArr  = csnones(kshape)

    scores:CsnArr = csnfftcorrelate(field, patch, 2)
    csnprint scores

    at:CsnArr     = csnargmax(scores)
    at_out:i[]    = csntoarray(csnflatten(at))
    prints("patch found at row %g, column %g\n", at_out[0], at_out[1])

    ; a kernel that is not symmetric separates correlation from convolution
    kernel:CsnArr = csnreshape(csnfromarray(array(1, 2, 3, 4)), kshape)
    corr:CsnArr   = csnfftcorrelate(field, kernel, 2)
    conv:CsnArr   = csnfftconvolve(field, kernel, 2)
    cell:i[]      = fillarray(0, 0)
    corr_00:i     = csnget(corr, cell)
    conv_00:i     = csnget(conv, cell)
    prints("correlate[0][0] = %g, convolve[0][0] = %g\n", corr_00, conv_00)

    ; and the direct form answers the same as this one
    direct:CsnArr = csncorrelate(field, kernel, 2)
    worst:i       = csnmax(csnabs(csnsubtract(direct, corr)))
    prints("largest difference from csncorrelate: %.2g\n", worst)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csncorrelate](csncorrelate.md)
* [csnfftconvolve](csnfftconvolve.md)
* [csnfftcorrelate1d](csnfftcorrelate1d.md)
* [csnargmax](csnargmax.md)

## Credits

Pasquale Mainolfi, 2026
