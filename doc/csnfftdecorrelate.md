# csnfftdecorrelate

## Abstract

N-D decorrelation by a kernel of the same rank, computed through Fourier transforms.

## Description

`csnfftdecorrelate` answers what [csndecorrelate](csndecorrelate.md) answers
through an N-D transform: every axis padded to the next power of two at or above
the source's length, the spectrum of `x` divided by `conj(H)`, and the answer
read `h.shape - 1` into the inverse transform on every axis.

As for [csnfftdeconvolve](csnfftdeconvolve.md), every bin of the kernel's
spectrum must be non-zero, and a kernel with a zero on the grid is refused;
bins below `1e-12` of the largest count as zeros.

The two routes agree when `x` really is a FULL correlation by `h`. When it is not
(noise added, a tail cut off) they part: the direct form reads only the first
`len(x) - len(h) + 1` samples and returns the quotient of a polynomial division,
the remainder discarded, while the transform divides the whole of `x`
circularly.

Everything else follows [csndecorrelate](csndecorrelate.md).

## Syntax

```csound
handle:CsnArr = csnfftdecorrelate(x:CsnArr, h:CsnArr)
handle:CsnArr = csnfftdecorrelate(x:CsnArr, h:CsnArr, trig:k)
```

## Arguments

* `x:CsnArr`: the FULL correlation to undo.
* `h:CsnArr`: the kernel; same rank as `x`, no axis longer than the matching one, at least one element, and a non-zero last element.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the decorrelated array, `x.shape - h.shape + 1`.

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
; csnfftdecorrelate.csd
;
; N-D decorrelation through Fourier transforms, checked against the direct
; form on the same operands.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]     = fillarray(3, 3)
    kshape:i[]    = fillarray(2, 2)
    field:CsnArr  = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8, 9)), shape)
    kernel:CsnArr = csnreshape(csnfromarray(array(0.5, 0.25, 1, 2)), kshape)
    corr:CsnArr   = csncorrelate(field, kernel, 0)

    back:CsnArr   = csnfftdecorrelate(corr, kernel)
    csnprint back

    direct:CsnArr = csndecorrelate(corr, kernel)
    worst:i       = csnmax(csnabs(csnsubtract(direct, back)))
    prints("largest difference from csndecorrelate: %.2g\n", worst)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csndecorrelate](csndecorrelate.md)
* [csnfftcorrelate](csnfftcorrelate.md)
* [csnfftdecorrelate1d](csnfftdecorrelate1d.md)
* [csnfftdeconvolve](csnfftdeconvolve.md)

## Credits

Pasquale Mainolfi, 2026
