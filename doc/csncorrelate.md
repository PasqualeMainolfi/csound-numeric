# csncorrelate

## Abstract

N-dimensional cross-correlation of an array with a kernel of the same rank.

## Description

`csncorrelate` slides a kernel shaped like the source over every axis at once
without flipping it, and sums the products. In two dimensions that is template
matching: the score is highest where the patch looks most like the kernel, so
the coordinates of the largest value, from [csnargmax](csnargmax.md), say where
the pattern sits. It is `scipy.signal.correlate`, and for a 2-D pair
`scipy.signal.correlate2d`.

The only difference from [csnconvolve](csnconvolve.md) is the kernel: here it
is used as it stands rather than flipped on every axis, and a complex kernel is
conjugated. On a kernel that is symmetric on every axis the two opcodes agree,
and on any other they do not.

Both arrays must have the same number of dimensions, and no axis of the kernel
may be longer than the matching axis of the source. Where the kernel is a
vector to be applied along one axis only, [csncorrelate1d](csncorrelate1d.md)
is the opcode for that.

The `edges` argument decides how much of the sliding is kept, axis by axis:

| edges | extent of each axis | meaning |
|-------|---------------------|---------|
| `0`   | `x[i] + h[i] - 1` | FULL: every position where the two overlap at all |
| `1`   | `x[i]`            | SAME: the shape of the source |
| `2`   | `x[i] - h[i] + 1` | VALID: only the positions where the kernel lies entirely inside the source |

VALID is the mode template matching wants: every score is computed from a full
overlap, so they are comparable with each other, and the coordinates of the
best one are the position of the match. Outside the source counts as zero, so
under SAME and FULL the scores near an edge are built from fewer contributions
and are not comparable in the same way.

An empty kernel is refused in every mode.

Both real and complex arrays are accepted, and a real operand is promoted when
the other is complex.

The edges mode is an init-time argument even on the k-rate form: it fixes the
shape of the output, and settling that at init is what keeps a performance pass
from having to reallocate.

## Syntax

```csound
handle:CsnArr = csncorrelate(x:CsnArr, h:CsnArr)
handle:CsnArr = csncorrelate(x:CsnArr, h:CsnArr, edges:i)
handle:CsnArr = csncorrelate(x:CsnArr, h:CsnArr, edges:i, trig:k)
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
; csncorrelate.csd
;
; Template matching in two dimensions: VALID gives one score per position the
; patch fits in, and the coordinates of the largest score are where it sits.
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

    scores:CsnArr = csncorrelate(field, patch, 2)
    csnprint scores

    at:CsnArr     = csnargmax(scores)
    at_out:i[]    = csntoarray(csnflatten(at))
    best:i        = csnmax(scores)
    prints("patch found at row %g, column %g, score %g\n", at_out[0], at_out[1], best)

    ; the flip is what separates the two: on a kernel that is not symmetric,
    ; correlation and convolution answer differently
    kernel:CsnArr = csnreshape(csnfromarray(array(1, 2, 3, 4)), kshape)
    corr:CsnArr   = csncorrelate(field, kernel, 2)
    conv:CsnArr   = csnconvolve(field, kernel, 2)
    cell:i[]      = fillarray(0, 0)
    corr_00:i     = csnget(corr, cell)
    conv_00:i     = csnget(conv, cell)
    prints("correlate[0][0] = %g, convolve[0][0] = %g\n", corr_00, conv_00)
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
* [csncorrelate1d](csncorrelate1d.md)
* [csnargmax](csnargmax.md)
* [csngetslice](csngetslice.md)

## Credits

Pasquale Mainolfi, 2026
