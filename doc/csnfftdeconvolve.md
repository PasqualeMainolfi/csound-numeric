# csnfftdeconvolve

## Abstract

N-D deconvolution by a kernel of the same rank, computed through Fourier transforms.

## Description

`csnfftdeconvolve` answers what [csndeconvolve](csndeconvolve.md) answers
through an N-D transform: every axis is padded to the next power of two at or
above the source's length on that axis, both operands are transformed, the
spectra divided and the quotient transformed back and cut to
`x.shape - h.shape + 1`.

Its kernel condition is the one on the spectrum rather than on the phase: every
bin must be non-zero. A 2 x 2 kernel with `h00 - h01 - h10 + h11 = 0`,
`[[1, 2], [3, 4]]` for instance, vanishes at Nyquist on both axes and is
refused. Bins below `1e-12` of the largest count as zeros.

The two routes agree when `x` really is a FULL convolution by `h`. When it is not
(noise added, a tail cut off) they part: the direct form reads only the first
`len(x) - len(h) + 1` samples and returns the quotient of a polynomial division,
the remainder discarded, while the transform divides the whole of `x`
circularly.

Everything else follows [csndeconvolve](csndeconvolve.md).

## Syntax

```csound
handle:CsnArr = csnfftdeconvolve(x:CsnArr, h:CsnArr)
handle:CsnArr = csnfftdeconvolve(x:CsnArr, h:CsnArr, trig:k)
```

## Arguments

* `x:CsnArr`: the FULL convolution to undo.
* `h:CsnArr`: the kernel; same rank as `x`, no axis longer than the matching one, at least one element, and a non-zero first element.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the deconvolved array, `x.shape - h.shape + 1`.

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
; csnfftdeconvolve.csd
;
; N-D deconvolution through Fourier transforms: every axis padded to a power
; of two at or above the source's length, spectra divided, transformed back.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]     = fillarray(3, 3)
    kshape:i[]    = fillarray(2, 2)
    image:CsnArr  = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8, 9)), shape)
    blur:CsnArr   = csnreshape(csnfromarray(array(1, 0.5, 0.5, 0.25)), kshape)
    blurred:CsnArr = csnconvolve(image, blur, 0)

    sharp:CsnArr  = csnfftdeconvolve(blurred, blur)
    csnprint sharp

    direct:CsnArr = csndeconvolve(blurred, blur)
    worst:i       = csnmax(csnabs(csnsubtract(direct, sharp)))
    prints("largest difference from csndeconvolve: %.2g\n", worst)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csndeconvolve](csndeconvolve.md)
* [csnfftconvolve](csnfftconvolve.md)
* [csnfftdeconvolve1d](csnfftdeconvolve1d.md)
* [csnfftdecorrelate](csnfftdecorrelate.md)

## Credits

Pasquale Mainolfi, 2026
