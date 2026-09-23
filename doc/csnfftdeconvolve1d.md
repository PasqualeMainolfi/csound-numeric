# csnfftdeconvolve1d

## Abstract

Deconvolution of an array by a 1-D kernel, computed through Fourier transforms.

## Description

`csnfftdeconvolve1d` answers the question [csndeconvolve1d](csndeconvolve1d.md)
answers by a different route: both operands are zero-padded to a common power
of two and transformed, the spectrum of `x` is divided by the spectrum of `h`,
and the quotient is transformed back and cut to `len(x) - len(h) + 1`.

It needs no recurrence, so it does not care whether the kernel is minimum
phase: where the direct form feeds its own rounding back and diverges, this one
stays at the rounding of a pair of transforms. The example shows both on the
same kernel.

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

The two routes agree when `x` really is a FULL convolution by `h`. When it is not
(noise added, a tail cut off) they part: the direct form reads only the first
`len(x) - len(h) + 1` samples and returns the quotient of a polynomial division,
the remainder discarded, while the transform divides the whole of `x`
circularly.

Everything else follows [csndeconvolve1d](csndeconvolve1d.md): no `edges`
argument, a 1-D kernel with a non-zero first element, the source read flat when
the axis is omitted, real and complex operands.

## Syntax

```csound
handle:CsnArr = csnfftdeconvolve1d(x:CsnArr, h:CsnArr)
handle:CsnArr = csnfftdeconvolve1d(x:CsnArr, h:CsnArr, axis:i)
handle:CsnArr = csnfftdeconvolve1d(x:CsnArr, h:CsnArr, trig:k)
handle:CsnArr = csnfftdeconvolve1d(x:CsnArr, h:CsnArr, trig:k, axis:i)
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
; csnfftdeconvolve1d.csd
;
; The same deconvolution through a pair of FFTs: the spectrum of y is divided
; by the spectrum of h. It needs no stable recurrence, only a kernel spectrum
; with no zero on the transform grid.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    dry:CsnArr    = csnfromarray(array(1, -2, 3, 0.5, 4, -1))
    echo:CsnArr   = csnfromarray(array(1, 0, 0, 0.5))
    wet:CsnArr    = csnconvolve1d(dry, echo, 0)

    back:CsnArr   = csnfftdeconvolve1d(wet, echo)
    back_out:i[]  = csntoarray(back)
    prints("recovered : %g %g %g %g %g %g\n", back_out[0], back_out[1], back_out[2], back_out[3], back_out[4], back_out[5])

    ; a kernel whose leading tap is not its largest: the direct recurrence
    ; divides by 0.5 at every step and amplifies its own rounding, the
    ; spectral division does not
    long_sig:CsnArr = csnsin(csnarange(0, 256, 1))
    ker:CsnArr      = csnfromarray(array(0.5, 1, 0.3))
    long_wet:CsnArr = csnconvolve1d(long_sig, ker, 0)
    viafft:CsnArr   = csnfftdeconvolve1d(long_wet, ker)
    direct:CsnArr   = csndeconvolve1d(long_wet, ker)
    err_fft:i       = csnmax(csnabs(csnsubtract(viafft, long_sig)))
    err_dir:i       = csnmax(csnabs(csnsubtract(direct, long_sig)))
    prints("256 samples: fft error %.2g, direct error %.2g\n", err_fft, err_dir)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csndeconvolve1d](csndeconvolve1d.md)
* [csnfftconvolve1d](csnfftconvolve1d.md)
* [csnfftdeconvolve](csnfftdeconvolve.md)
* [csnfftdecorrelate1d](csnfftdecorrelate1d.md)

## Credits

Pasquale Mainolfi, 2026
