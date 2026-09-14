# csncorrelate1d

## Abstract

Cross-correlation of an array with a 1-D kernel, flat or along one axis.

## Description

`csncorrelate1d` slides a kernel over a signal without flipping it and sums the
products: `y[n] = sum over j of x[n + j] * conj(h[j])`. That is the matched
filter — the largest answer is the position where the kernel best resembles the
signal — and it is also how a lag is measured between two signals. It is
`np.correlate` with an axis argument.

The only difference from [csnconvolve1d](csnconvolve1d.md) is the kernel: here
it is read back to front, and a complex kernel is conjugated. On a symmetric
real kernel the two opcodes therefore agree, and on any other they do not.

The kernel must be one-dimensional. With the axis omitted, the source is
read flat; with an axis, every lane along that axis is correlated on its own.

The `edges` argument decides how much of the sliding is kept:

| edges | length (1-D) | meaning |
|-------|--------------|---------|
| `0`   | `len(x) + len(h) - 1` | FULL: every position where the two overlap at all |
| `1`   | `len(x)`              | SAME: as many values as the source had, centred on it |
| `2`   | `len(x) - len(h) + 1` | VALID: only the positions where the kernel lies entirely inside the source |

These are NumPy's three modes and the lengths match. VALID is the one to reach
for when matching a template: it gives exactly one score per position the
template fits in, so the index of the largest, from
[csnargmax](csnargmax.md), is where the template starts. VALID requires the
source to be at least as long as the kernel, along the axis being correlated,
and an empty kernel is refused in every mode.

For a kernel shaped like the source rather than laid along one axis, see
[csncorrelate](csncorrelate.md).

Both real and complex arrays are accepted, and a real operand is promoted when
the other is complex.

The edges mode and the axis are init-time arguments even on the k-rate form:
they fix the shape of the output, and settling that at init is what keeps a
performance pass from having to reallocate.

## Syntax

```csound
handle:CsnArr = csncorrelate1d(x:CsnArr, h:CsnArr)
handle:CsnArr = csncorrelate1d(x:CsnArr, h:CsnArr, edges:i)
handle:CsnArr = csncorrelate1d(x:CsnArr, h:CsnArr, edges:i, axis:i)
handle:CsnArr = csncorrelate1d(x:CsnArr, h:CsnArr, edges:i, trig:k)
handle:CsnArr = csncorrelate1d(x:CsnArr, h:CsnArr, edges:i, trig:k, axis:i)
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
; csncorrelate1d.csd
;
; Correlation is convolution with the kernel read back to front, which is what
; makes it the matched filter: slide a template along a signal and the largest
; answer is where the template sits.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    signal:CsnArr = csnfromarray(array(1, 2, 3, 4, 5))
    kernel:CsnArr = csnfromarray(array(1, 0.5, 0.25))

    ; the same operands csnconvolve1d uses, so the two can be compared directly
    corr:CsnArr   = csncorrelate1d(signal, kernel, 0)
    corr_out:i[]  = csntoarray(corr)
    conv:CsnArr   = csnconvolve1d(signal, kernel, 0)
    conv_out:i[]  = csntoarray(conv)
    prints("correlate : %g %g %g %g %g %g %g\n", corr_out[0], corr_out[1], corr_out[2], corr_out[3], corr_out[4], corr_out[5], corr_out[6])
    prints("convolve  : %g %g %g %g %g %g %g\n", conv_out[0], conv_out[1], conv_out[2], conv_out[3], conv_out[4], conv_out[5], conv_out[6])

    ; matched filter: VALID gives one answer per position the template fits in,
    ; so the index of the largest is where the template starts
    field:CsnArr    = csnfromarray(array(0, 0, 1, 2, 1, 0, 0))
    template:CsnArr = csnfromarray(array(1, 2, 1))

    scores:CsnArr   = csncorrelate1d(field, template, 2)
    scores_out:i[]  = csntoarray(scores)
    scores_n:i      = csnsize(scores)
    prints("scores : n = %d : %g %g %g %g %g\n", scores_n, scores_out[0], scores_out[1], scores_out[2], scores_out[3], scores_out[4])

    at:CsnArr       = csnargmax(scores)
    at_out:i[]      = csntoarray(csnflatten(at))
    peak:i          = csnmax(scores)
    prints("template starts at index %g, score %g\n", at_out[0], peak)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnconvolve1d](csnconvolve1d.md)
* [csncorrelate](csncorrelate.md)
* [csnargmax](csnargmax.md)
* [csnangledist](csnangledist.md)

## Credits

Pasquale Mainolfi, 2026
