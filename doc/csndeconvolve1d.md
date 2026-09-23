# csndeconvolve1d

## Abstract

Deconvolution of an array by a 1-D kernel, flat or along one axis.

## Description

`csndeconvolve1d` undoes [csnconvolve1d](csnconvolve1d.md) in its FULL mode:
given `x = y * h` and `h`, it returns `y`. It runs the convolution sum
backwards, one sample at a time,

```
y[n] = (x[n] - sum over j >= 1 of h[j] * y[n - j]) / h[0]
```

so every output is the input minus what the earlier outputs already explain,
divided by the leading tap. That is polynomial long division, and the answer is
the quotient `scipy.signal.deconvolve` returns, with an axis argument.

There is no `edges` argument. The forward form's FULL output is the only one
that keeps every sample the source contributed to, so it is the only one that
can be undone: the answer is `len(x) - len(h) + 1` long, and a source shorter
than the kernel is refused.

The leading tap must be non-zero, since every step divides by it. The
recurrence is also a recursive filter whose poles are the roots of `h`: it is
stable when `h` is minimum phase (every root inside the unit circle), which is
guaranteed whenever `|h[0]|` exceeds the sum of the other taps' magnitudes, as
for an echo. For any other kernel the
rounding of each step is fed back and grows, and on a long source it swamps
the answer; [csnfftdeconvolve1d](csnfftdeconvolve1d.md) has no such limit.

With the axis omitted the source is read flat; with an axis, every lane along
it is deconvolved on its own. Both real and complex arrays are accepted, and a
real operand is promoted when the other is complex. The axis is an init-time
argument even on the k-rate form, where the trigger comes before it.

[csndeconvolve](csndeconvolve.md) takes a kernel shaped like the source, and
[csndecorrelate1d](csndecorrelate1d.md) undoes a correlation instead.

## Syntax

```csound
handle:CsnArr = csndeconvolve1d(x:CsnArr, h:CsnArr)
handle:CsnArr = csndeconvolve1d(x:CsnArr, h:CsnArr, axis:i)
handle:CsnArr = csndeconvolve1d(x:CsnArr, h:CsnArr, trig:k)
handle:CsnArr = csndeconvolve1d(x:CsnArr, h:CsnArr, trig:k, axis:i)
```

## Arguments

* `x:CsnArr`: the FULL convolution to undo.
* `h:CsnArr`: the kernel; must be 1-D, hold at least one element, be no longer than `x` along the axis, and have a non-zero first element.
* `axis:i` (optional): the axis to work along. Omit it to read the array flat; `-1` selects the last axis.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the deconvolved array, `len(x) - len(h) + 1` long along the axis.

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
; csndeconvolve1d.csd
;
; Deconvolution undoes a FULL convolution: given y = x * h and h, it gives x
; back, one sample per position the kernel fits in, len(y) - len(h) + 1. Here
; it takes an echo back out of a signal.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    dry:CsnArr    = csnfromarray(array(1, -2, 3, 0.5, 4, -1))
    echo:CsnArr   = csnfromarray(array(1, 0, 0, 0.5))
    wet:CsnArr    = csnconvolve1d(dry, echo, 0)
    wet_out:i[]   = csntoarray(wet)
    prints("with echo : %g %g %g %g %g %g %g %g %g\n", wet_out[0], wet_out[1], wet_out[2], wet_out[3], wet_out[4], wet_out[5], wet_out[6], wet_out[7], wet_out[8])

    back:CsnArr   = csndeconvolve1d(wet, echo)
    back_out:i[]  = csntoarray(back)
    back_n:i      = csnsize(back)
    prints("recovered : n = %d : %g %g %g %g %g %g\n", back_n, back_out[0], back_out[1], back_out[2], back_out[3], back_out[4], back_out[5])

    ; along an axis, every lane is recovered on its own
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    kernel:CsnArr = csnfromarray(array(2, 1))
    rows:CsnArr   = csnconvolve1d(mat, kernel, 0, -1)
    csnprint rows
    undone:CsnArr = csndeconvolve1d(rows, kernel, -1)
    csnprint undone
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
* [csnfftdeconvolve1d](csnfftdeconvolve1d.md)
* [csndeconvolve](csndeconvolve.md)
* [csndecorrelate1d](csndecorrelate1d.md)

## Credits

Pasquale Mainolfi, 2026
