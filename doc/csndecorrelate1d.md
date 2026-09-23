# csndecorrelate1d

## Abstract

Decorrelation of an array by a 1-D kernel, flat or along one axis.

## Description

`csndecorrelate1d` undoes [csncorrelate1d](csncorrelate1d.md) in its FULL mode:
given `c = correlate(y, h)` and `h`, it returns `y`. A correlation is a
convolution with the kernel reversed and conjugated, so this is
[csndeconvolve1d](csndeconvolve1d.md) with that kernel: the same recurrence,
divided at every step by `conj(h[last])` instead of `h[0]`. In SciPy terms it
is the quotient of `scipy.signal.deconvolve(c, np.conj(h[::-1]))`.

There is no `edges` argument. The forward form's FULL output is the only one
that keeps every sample the source contributed to, so it is the only one that
can be undone: the answer is `len(x) - len(h) + 1` long, and a source shorter
than the kernel is refused.

The last tap must be non-zero. Stability reverses too: the recurrence is stable
when the reversed kernel is minimum phase, guaranteed whenever the last tap's
magnitude exceeds the sum of the others'.
A kernel that suits [csndeconvolve1d](csndeconvolve1d.md) is the wrong way
round here, and the reverse. [csnfftdecorrelate1d](csnfftdecorrelate1d.md) has
no such limit.

The example also deconvolves the same correlation, which reads the kernel the
wrong way round and returns something else entirely.

With the axis omitted the source is read flat; with an axis, every lane along
it is decorrelated on its own. Both real and complex arrays are accepted, and a
real operand is promoted when the other is complex. The axis is an init-time
argument even on the k-rate form, where the trigger comes before it.

## Syntax

```csound
handle:CsnArr = csndecorrelate1d(x:CsnArr, h:CsnArr)
handle:CsnArr = csndecorrelate1d(x:CsnArr, h:CsnArr, axis:i)
handle:CsnArr = csndecorrelate1d(x:CsnArr, h:CsnArr, trig:k)
handle:CsnArr = csndecorrelate1d(x:CsnArr, h:CsnArr, trig:k, axis:i)
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
; csndecorrelate1d.csd
;
; Decorrelation undoes a FULL correlation: given c = correlate(x, h) and h, it
; gives x back. The correlation reads the kernel reversed and conjugated, so
; the pivot of the recurrence is conj(h[last]) rather than h[0].
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    signal:CsnArr = csnfromarray(array(1, -2, 3, 0.5, 4))
    kernel:CsnArr = csnfromarray(array(0.25, 0.5, 1))

    corr:CsnArr   = csncorrelate1d(signal, kernel, 0)
    corr_out:i[]  = csntoarray(corr)
    prints("correlated : %g %g %g %g %g %g %g\n", corr_out[0], corr_out[1], corr_out[2], corr_out[3], corr_out[4], corr_out[5], corr_out[6])

    back:CsnArr   = csndecorrelate1d(corr, kernel)
    back_out:i[]  = csntoarray(back)
    back_n:i      = csnsize(back)
    prints("recovered  : n = %d : %g %g %g %g %g\n", back_n, back_out[0], back_out[1], back_out[2], back_out[3], back_out[4])

    ; deconvolving the same c instead treats the kernel the wrong way round
    wrong:CsnArr  = csndeconvolve1d(corr, kernel)
    wrong_out:i[] = csntoarray(wrong)
    prints("deconvolved: %g %g %g %g %g\n", wrong_out[0], wrong_out[1], wrong_out[2], wrong_out[3], wrong_out[4])
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
* [csnfftdecorrelate1d](csnfftdecorrelate1d.md)
* [csndecorrelate](csndecorrelate.md)
* [csndeconvolve1d](csndeconvolve1d.md)

## Credits

Pasquale Mainolfi, 2026
