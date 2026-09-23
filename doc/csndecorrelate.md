# csndecorrelate

## Abstract

N-D decorrelation by a kernel of the same rank.

## Description

`csndecorrelate` undoes a FULL [csncorrelate](csncorrelate.md), with a kernel
shaped like the source: [csndeconvolve](csndeconvolve.md) run with the kernel
reversed on every axis and conjugated, so each element is divided by the
conjugate of the kernel's last element. The answer is `x.shape - h.shape + 1`.

There is no `edges` argument. The kernel must have the rank of the source, no
axis longer than the matching one, and a non-zero last element. The recurrence
is stable when the reversed kernel is minimum phase in every direction;
[csnfftdecorrelate](csnfftdecorrelate.md) has no such limit.

The example prints the convolution of the same operands as well: on a kernel
that is not symmetric it differs, and it is the correlation that
`csndecorrelate` undoes.

Both real and complex arrays are accepted, and a real operand is promoted when
the other is complex. SciPy has no N-D counterpart.

## Syntax

```csound
handle:CsnArr = csndecorrelate(x:CsnArr, h:CsnArr)
handle:CsnArr = csndecorrelate(x:CsnArr, h:CsnArr, trig:k)
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
; csndecorrelate.csd
;
; N-D decorrelation: the inverse of a FULL csncorrelate, with a kernel shaped
; like the source. The pivot is the conjugate of the kernel's last element.
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
    csnprint corr

    back:CsnArr   = csndecorrelate(corr, kernel)
    csnprint back

    ; the same kernel convolved gives a different c: the flip is on every axis
    conv:CsnArr   = csnconvolve(field, kernel, 0)
    csnprint conv
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
* [csndecorrelate1d](csndecorrelate1d.md)
* [csnfftdecorrelate](csnfftdecorrelate.md)
* [csndeconvolve](csndeconvolve.md)

## Credits

Pasquale Mainolfi, 2026
