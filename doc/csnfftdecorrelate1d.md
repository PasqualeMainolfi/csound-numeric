# csnfftdecorrelate1d

## Abstract

Decorrelation of an array by a 1-D kernel, computed through Fourier transforms.

## Description

`csnfftdecorrelate1d` answers what [csndecorrelate1d](csndecorrelate1d.md)
answers by dividing spectra: the spectrum of `x` is divided by `conj(H)`, the
spectrum a correlation multiplies by, and the answer is read `len(h) - 1`
samples into the inverse transform, where the reversed kernel left it.

Like [csnfftdeconvolve1d](csnfftdeconvolve1d.md) it needs no recurrence and
does not depend on the phase of the kernel.

The transform length is derived, not asked for: it is the next power of two at
or above the length of `x` along the axis, since `x` already holds the FULL
answer and the circular product of the answer with `h` fits in it without
wrapping. At k-rate it follows the operands.

A spectral division needs every bin of the kernel's spectrum. A kernel with a
zero on the unit circle that lands on the transform grid has thrown that bin of
the answer away, and dividing by what rounding leaves there would return noise
in its place. `[1, 1]` is the common case: it vanishes at Nyquist, which is a
grid bin at every even length. Such a kernel is refused, at init or at
performance, rather than answered wrongly; bins below `1e-12` of the largest
count as zeros. The direct form never looks at the spectrum and still answers
for it.

The two routes agree when `x` really is a FULL correlation by `h`. When it is not
(noise added, a tail cut off) they part: the direct form reads only the first
`len(x) - len(h) + 1` samples and returns the quotient of a polynomial division,
the remainder discarded, while the transform divides the whole of `x`
circularly.

Everything else follows [csndecorrelate1d](csndecorrelate1d.md): no `edges`
argument, a 1-D kernel with a non-zero last element, the source read flat when
the axis is omitted, real and complex operands.

## Syntax

```csound
handle:CsnArr = csnfftdecorrelate1d(x:CsnArr, h:CsnArr)
handle:CsnArr = csnfftdecorrelate1d(x:CsnArr, h:CsnArr, axis:i)
handle:CsnArr = csnfftdecorrelate1d(x:CsnArr, h:CsnArr, trig:k)
handle:CsnArr = csnfftdecorrelate1d(x:CsnArr, h:CsnArr, trig:k, axis:i)
```

## Arguments

* `x:CsnArr`: the FULL correlation to undo.
* `h:CsnArr`: the kernel; must be 1-D, hold at least one element, be no longer than `x` along the axis, and have a non-zero last element.
* `axis:i` (optional): the axis to work along. Omit it to read the array flat; `-1` selects the last axis.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the decorrelated array, `len(x) - len(h) + 1` long along the axis.

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
; csnfftdecorrelate1d.csd
;
; The same decorrelation through a pair of FFTs: the spectrum is divided by
; conj(H), and the answer is read h - 1 samples into the inverse transform,
; where the reversed kernel left it.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    signal:CsnArr = csnfromarray(array(1, -2, 3, 0.5, 4))
    kernel:CsnArr = csnfromarray(array(0.25, 0.5, 1))
    corr:CsnArr   = csncorrelate1d(signal, kernel, 0)

    back:CsnArr   = csnfftdecorrelate1d(corr, kernel)
    back_out:i[]  = csntoarray(back)
    prints("recovered : %g %g %g %g %g\n", back_out[0], back_out[1], back_out[2], back_out[3], back_out[4])

    ; along an axis of a matrix, against the direct form
    shape:i[]     = fillarray(2, 4)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, -1, 0, 2, 5)), shape)
    lanes:CsnArr  = csncorrelate1d(mat, kernel, 0, -1)
    viafft:CsnArr = csnfftdecorrelate1d(lanes, kernel, -1)
    direct:CsnArr = csndecorrelate1d(lanes, kernel, -1)
    csnprint viafft
    worst:i       = csnmax(csnabs(csnsubtract(direct, viafft)))
    prints("largest difference from csndecorrelate1d: %.2g\n", worst)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csndecorrelate1d](csndecorrelate1d.md)
* [csnfftcorrelate1d](csnfftcorrelate1d.md)
* [csnfftdecorrelate](csnfftdecorrelate.md)
* [csnfftdeconvolve1d](csnfftdeconvolve1d.md)

## Credits

Pasquale Mainolfi, 2026
