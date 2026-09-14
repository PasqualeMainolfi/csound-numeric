# csnfftcorrelate1d

## Abstract

Cross-correlation of an array with a 1-D kernel, computed through Fourier transforms.

## Description

`csnfftcorrelate1d` is [csncorrelate1d](csncorrelate1d.md) by way of the
spectrum: both operands are zero-padded to a common power of two, transformed,
multiplied with the kernel's spectrum conjugated, and transformed back.
Conjugating in the spectrum is the same operation as reading the kernel
backwards in time, said the other way round. It is
`scipy.signal.correlate(..., method='fft')` with an axis argument.

The reason to reach for it is length: the direct form costs one multiply-add
per output sample per tap, this one costs `n log n` in the padded length
whatever the kernel does. Below a dozen taps the direct form is faster, above a
few hundred there is no contest. The two agree to within floating-point
rounding, not bit for bit.

Matched filtering is the usual use, and VALID is the mode for it: every score
comes from a full overlap, so the scores are comparable with each other and the
index of the largest - from [csnargmax](csnargmax.md) - is where the template
starts.

Everything else follows [csncorrelate1d](csncorrelate1d.md): the kernel must be
1-D and non-empty, omitting the axis reads the source flat, and `edges`
selects FULL, SAME or VALID with the same lengths. Both real and complex arrays
are accepted.

The transform length is the next power of two at or above `x + h - 1` along the
axis, and at k-rate it follows the operands rather than staying at whatever the
init pass needed.

[csnfftcorrelate](csnfftcorrelate.md) takes a kernel shaped like the source
instead of one laid along an axis.

## Syntax

```csound
handle:CsnArr = csnfftcorrelate1d(x:CsnArr, h:CsnArr)
handle:CsnArr = csnfftcorrelate1d(x:CsnArr, h:CsnArr, edges:i)
handle:CsnArr = csnfftcorrelate1d(x:CsnArr, h:CsnArr, edges:i, axis:i)
handle:CsnArr = csnfftcorrelate1d(x:CsnArr, h:CsnArr, edges:i, trig:k)
handle:CsnArr = csnfftcorrelate1d(x:CsnArr, h:CsnArr, edges:i, trig:k, axis:i)
```

## Arguments

* `x:CsnArr`: the array to correlate.
* `h:CsnArr`: the kernel; must be 1-D and hold at least one element.
* `edges:i` (optional, default `0`): `0` FULL, `1` SAME, `2` VALID.
* `axis:i` (optional): the axis to correlate along. Omit it to read the array flat; `-1` selects the last axis.
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
; csnfftcorrelate1d.csd
;
; Correlation through a pair of FFTs. The kernel is conjugated in the spectrum
; instead of being read backwards in time, which is the same operation said the
; other way round: the answers match csncorrelate1d to within rounding.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    signal:CsnArr = csnfromarray(array(1, 2, 3, 4, 5))
    kernel:CsnArr = csnfromarray(array(1, 0.5, 0.25))

    corr:CsnArr   = csnfftcorrelate1d(signal, kernel, 0)
    corr_out:i[]  = csntoarray(corr)
    conv:CsnArr   = csnfftconvolve1d(signal, kernel, 0)
    conv_out:i[]  = csntoarray(conv)
    prints("correlate : %g %g %g %g %g %g %g\n", corr_out[0], corr_out[1], corr_out[2], corr_out[3], corr_out[4], corr_out[5], corr_out[6])
    prints("convolve  : %g %g %g %g %g %g %g\n", conv_out[0], conv_out[1], conv_out[2], conv_out[3], conv_out[4], conv_out[5], conv_out[6])

    ; matched filter over a long signal: VALID scores one position per place
    ; the template fits, and the largest is where it sits
    fieldshape:i[]  = fillarray(256)
    template:CsnArr = csnhanning(64)
    field:CsnArr    = csnzeros(fieldshape)
    csnsetslice(field, template, 0, 100, 164, 1)

    scores:CsnArr   = csnfftcorrelate1d(field, template, 2)
    at:CsnArr       = csnargmax(scores)
    at_out:i[]      = csntoarray(csnflatten(at))
    prints("template planted at 100, found at %g\n", at_out[0])

    ; the direct form answers the same thing
    direct:CsnArr   = csncorrelate1d(field, template, 2)
    worst:i         = csnmax(csnabs(csnsubtract(direct, scores)))
    prints("largest difference from csncorrelate1d: %.2g\n", worst)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csncorrelate1d](csncorrelate1d.md)
* [csnfftconvolve1d](csnfftconvolve1d.md)
* [csnfftcorrelate](csnfftcorrelate.md)
* [csnargmax](csnargmax.md)

## Credits

Pasquale Mainolfi, 2026
