# csnconvolve

## Abstract

N-dimensional convolution of an array with a kernel of the same rank.

## Description

`csnconvolve` convolves over every axis at once with a kernel shaped like the
source: a 2-D kernel over a matrix is the image-processing form, where a blur,
a sharpen or an edge detector is a small matrix slid over a larger one. It is
`scipy.signal.convolve`, and for a 2-D pair `scipy.signal.convolve2d`.

The kernel is flipped on **every** axis, which is what makes this convolution
rather than correlation, and a complex kernel is used as it stands.
[csncorrelate](csncorrelate.md) is the unflipped, conjugated counterpart.

Both arrays must have the same number of dimensions, and no axis of the kernel
may be longer than the matching axis of the source. Where the kernel is a
vector to be applied along one axis only, [csnconvolve1d](csnconvolve1d.md) is
the opcode for that, and it leaves the other axes untouched.

The `edges` argument decides how much of the sliding is kept, axis by axis:

| edges | extent of each axis | meaning |
|-------|---------------------|---------|
| `0`   | `x[i] + h[i] - 1` | FULL: every position where the two overlap at all |
| `1`   | `x[i]`            | SAME: the shape of the source, so a filter gives back what it was given |
| `2`   | `x[i] - h[i] + 1` | VALID: only the positions where the kernel lies entirely inside the source |

Outside the source counts as zero, so under SAME and FULL the values near an
edge are computed from fewer contributions than the ones in the middle. A
kernel whose values sum to one leaves an interior level unchanged and still
darkens the border for that reason.

An empty kernel is refused in every mode.

Both real and complex arrays are accepted, and a real operand is promoted when
the other is complex.

The edges mode is an init-time argument even on the k-rate form: it fixes the
shape of the output, and settling that at init is what keeps a performance pass
from having to reallocate.

## Syntax

```csound
handle:CsnArr = csnconvolve(x:CsnArr, h:CsnArr)
handle:CsnArr = csnconvolve(x:CsnArr, h:CsnArr, edges:i)
handle:CsnArr = csnconvolve(x:CsnArr, h:CsnArr, edges:i, trig:k)
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
; csnconvolve.csd
;
; The N-D form takes a kernel shaped like the source and flips it on every axis
; at once. SAME is the mode a filter usually wants: the result keeps the shape
; of what went in.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]      = fillarray(3, 3)
    kshape:i[]     = fillarray(2, 2)
    source:CsnArr  = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8, 9)), shape)
    kernel:CsnArr  = csnreshape(csnfromarray(array(1, 2, 3, 4)), kshape)

    full:CsnArr    = csnconvolve(source, kernel, 0)
    csnprint full

    ; SAME keeps the source shape, VALID only the fully covered positions
    same:CsnArr    = csnconvolve(source, kernel, 1)
    same_shape:i[] = csnshape(same)
    prints("same  shape : %d x %d\n", same_shape[0], same_shape[1])

    valid:CsnArr   = csnconvolve(source, kernel, 2)
    valid_shape:i[] = csnshape(valid)
    prints("valid shape : %d x %d\n", valid_shape[0], valid_shape[1])
    csnprint valid

    ; a 2x2 box blur: the kernel sums to one, so an interior value comes back as
    ; the average of its neighbourhood. Outside the source counts as zero, which
    ; is why the edges - and with them the overall mean - fall off.
    box:CsnArr     = csnfull(kshape, 0.25)
    blurred:CsnArr = csnconvolve(source, box, 1)
    centre:i[]     = fillarray(1, 1)
    centre_in:i    = csnget(source, centre)
    centre_out:i   = csnget(blurred, centre)
    prints("centre %g -> %g (mean of 1, 2, 4, 5)\n", centre_in, centre_out)
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
* [csnconvolve1d](csnconvolve1d.md)
* [csnfft2](csnfft2.md)
* [csnpad](csnpad.md)

## Credits

Pasquale Mainolfi, 2026
