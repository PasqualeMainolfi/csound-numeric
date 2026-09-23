# csndeconvolve

## Abstract

N-D deconvolution by a kernel of the same rank.

## Description

`csndeconvolve` undoes a FULL [csnconvolve](csnconvolve.md): the kernel is
shaped like the source, and the answer is `x.shape - h.shape + 1` on every
axis. It runs the same recurrence as [csndeconvolve1d](csndeconvolve1d.md) in
N dimensions, visiting the output in row-major order: every kernel offset other
than the origin points at an output already written, so each element is the
source minus what those explain, divided by the kernel's first element.

There is no `edges` argument, for the reason given on the 1-D page. The kernel
must have the rank of the source, no axis longer than the matching one, and a
non-zero first element. The recurrence is stable when the kernel is minimum
phase in every direction; [csnfftdeconvolve](csnfftdeconvolve.md) has no such
limit.

Both real and complex arrays are accepted, and a real operand is promoted when
the other is complex. SciPy has no N-D counterpart.

## Syntax

```csound
handle:CsnArr = csndeconvolve(x:CsnArr, h:CsnArr)
handle:CsnArr = csndeconvolve(x:CsnArr, h:CsnArr, trig:k)
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
; csndeconvolve.csd
;
; N-D deconvolution: a kernel shaped like the source, undone on every axis at
; once. The answer is x.shape - h.shape + 1 on each axis.
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
    csnprint blurred

    sharp:CsnArr  = csndeconvolve(blurred, blur)
    csnprint sharp
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
* [csndeconvolve1d](csndeconvolve1d.md)
* [csnfftdeconvolve](csnfftdeconvolve.md)
* [csndecorrelate](csndecorrelate.md)

## Credits

Pasquale Mainolfi, 2026
