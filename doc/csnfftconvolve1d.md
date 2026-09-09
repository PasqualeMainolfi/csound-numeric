# csnfftconvolve1d

## Abstract

Discrete convolution of an array with a 1-D kernel, computed through Fourier transforms.

## Description

`csnfftconvolve1d` answers the same question as
[csnconvolve1d](csnconvolve1d.md) - the same operands, the same `edges` modes,
the same lengths - by a different route: both operands are zero-padded to a
common power of two, transformed, multiplied in the spectrum, and transformed
back. It is `scipy.signal.fftconvolve` with an axis argument.

What changes is the cost. The direct form does one multiply-add per output
sample per tap, so it grows with the product of the two lengths; this one grows
as `n log n` in the padded length whatever the kernel does. For a handful of
taps the direct form wins on its lower constant, and somewhere around a dozen
the transform takes over; by a few hundred taps there is no contest. Neither
form is the right answer for every case, which is why both exist.

The results are equal to within floating-point rounding, not bit for bit: the
transform accumulates a different sequence of roundings. On the examples below
the largest difference is a few units in the last place.

Everything else follows [csnconvolve1d](csnconvolve1d.md): the kernel must be
1-D and non-empty, the default axis `-1` reads the source flat, `edges`
selects `0` FULL, `1` SAME, `2` VALID, and VALID requires the source to be at
least as long as the kernel along the axis being convolved. Both real and
complex arrays are accepted, and a real operand is promoted when the other is
complex.

The transform length is derived, not asked for: it is the next power of two at
or above `x + h - 1` along the axis. At k-rate it follows the operands, so a
source or a kernel that changes shape gets a transform sized for it rather than
an answer aliased around a circle too small.

[csnfftcorrelate1d](csnfftcorrelate1d.md) is the correlation counterpart, and
[csnfftconvolve](csnfftconvolve.md) takes a kernel shaped like the source
instead of one laid along an axis.

## Syntax

```csound
handle:CsnArr = csnfftconvolve1d(x:CsnArr, h:CsnArr)
handle:CsnArr = csnfftconvolve1d(x:CsnArr, h:CsnArr, edges:i)
handle:CsnArr = csnfftconvolve1d(x:CsnArr, h:CsnArr, edges:i, axis:i)
handle:CsnArr = csnfftconvolve1d(x:CsnArr, h:CsnArr, edges:i, axis:i, trig:k)
```

## Arguments

* `x:CsnArr`: the array to convolve.
* `h:CsnArr`: the kernel; must be 1-D and hold at least one element.
* `edges:i` (optional, default `0`): `0` FULL, `1` SAME, `2` VALID.
* `axis:i` (optional, default `-1`): the axis to convolve along; `-1` reads the array flat.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the convolution.

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
; csnfftconvolve1d.csd
;
; The same convolution csnconvolve1d computes, routed through a pair of FFTs.
; Same operands, same edge modes, same answers: what changes is the cost, which
; stops growing with the kernel once the kernel is long.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    signal:CsnArr = csnfromarray(array(1, 2, 3, 4, 5))
    kernel:CsnArr = csnfromarray(array(1, 0.5, 0.25))

    full:CsnArr   = csnfftconvolve1d(signal, kernel, 0)
    full_out:i[]  = csntoarray(full)
    prints("full  : %g %g %g %g %g %g %g\n", full_out[0], full_out[1], full_out[2], full_out[3], full_out[4], full_out[5], full_out[6])

    same:CsnArr   = csnfftconvolve1d(signal, kernel, 1)
    same_out:i[]  = csntoarray(same)
    prints("same  : %g %g %g %g %g\n", same_out[0], same_out[1], same_out[2], same_out[3], same_out[4])

    valid:CsnArr  = csnfftconvolve1d(signal, kernel, 2)
    valid_out:i[] = csntoarray(valid)
    prints("valid : %g %g %g\n", valid_out[0], valid_out[1], valid_out[2])

    ; the answer is the direct one to within rounding: a 512-sample signal
    ; through a 200-tap kernel, the length where the transform earns its keep
    long_sig:CsnArr = csnsin(csnarange(0, 512, 1))
    long_ker:CsnArr = csnhanning(200)

    direct:CsnArr   = csnconvolve1d(long_sig, long_ker, 0)
    viafft:CsnArr   = csnfftconvolve1d(long_sig, long_ker, 0)
    n_direct:i      = csnsize(direct)
    n_fft:i         = csnsize(viafft)
    worst:i         = csnmax(csnabs(csnsubtract(direct, viafft)))
    prints("512 x 200 taps: n = %d and %d, largest difference %.2g\n", n_direct, n_fft, worst)

    ; along an axis of a matrix, exactly as the direct form does
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    ones:CsnArr   = csnfromarray(array(1, 1))
    rows:CsnArr   = csnfftconvolve1d(mat, ones, 0, 1)
    csnprint rows
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
* [csnfftcorrelate1d](csnfftcorrelate1d.md)
* [csnfftconvolve](csnfftconvolve.md)
* [csnrfft](csnrfft.md)

## Credits

Pasquale Mainolfi, 2026
