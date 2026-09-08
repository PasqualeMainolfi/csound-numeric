# csnconvolve1d

## Abstract

Discrete convolution of an array with a 1-D kernel, flat or along one axis.

## Description

`csnconvolve1d` slides a kernel over a signal and sums the products at every
position: `y[n] = sum over j of x[n - j] * h[j]`, which is what a FIR filter,
an envelope smoother or an impulse response applied by hand all compute. It is
`np.convolve` with an axis argument.

The kernel must be one-dimensional. With the default axis, `-1`, the source is
read flat and the result is a vector. With an axis, every lane along that axis
is convolved on its own and the others are left alone, so a 2×3 matrix
convolved along axis 0 comes back with three columns, each the convolution of
the column it came from.

The `edges` argument decides how much of the sliding is kept:

| edges | length (1-D) | meaning |
|-------|--------------|---------|
| `0`   | `len(x) + len(h) - 1` | FULL: every position where the two overlap at all, so the tails are included |
| `1`   | `len(x)`              | SAME: as many values as the source had, centred on it |
| `2`   | `len(x) - len(h) + 1` | VALID: only the positions where the kernel lies entirely inside the source |

These are NumPy's three modes and the lengths match. VALID requires the source
to be at least as long as the kernel, along the axis being convolved, and an
empty kernel is refused in every mode.

[csncorrelate1d](csncorrelate1d.md) is the same operation with the kernel read
back to front, and for a complex kernel, conjugated. For a kernel shaped like
the source rather than laid along one axis, see
[csnconvolve](csnconvolve.md).

Both real and complex arrays are accepted, and a real operand is promoted when
the other is complex.

The edges mode and the axis are init-time arguments even on the k-rate form:
they fix the shape of the output, and settling that at init is what keeps a
performance pass from having to reallocate.

## Syntax

```csound
handle:CsnArr = csnconvolve1d(x:CsnArr, h:CsnArr)
handle:CsnArr = csnconvolve1d(x:CsnArr, h:CsnArr, edges:i)
handle:CsnArr = csnconvolve1d(x:CsnArr, h:CsnArr, edges:i, axis:i)
handle:CsnArr = csnconvolve1d(x:CsnArr, h:CsnArr, edges:i, axis:i, trig:k)
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
; csnconvolve1d.csd
;
; The three edge modes are the same convolution read over different spans: FULL
; keeps every position where the kernel touches the signal, SAME keeps as many
; as the signal had, VALID only the ones where the kernel sits entirely inside.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    signal:CsnArr = csnfromarray(array(1, 2, 3, 4, 5))
    kernel:CsnArr = csnfromarray(array(1, 0.5, 0.25))

    full:CsnArr   = csnconvolve1d(signal, kernel, 0)
    full_out:i[]  = csntoarray(full)
    full_n:i      = csnsize(full)
    prints("full  : n = %d : %g %g %g %g %g %g %g\n", full_n, full_out[0], full_out[1], full_out[2], full_out[3], full_out[4], full_out[5], full_out[6])

    same:CsnArr   = csnconvolve1d(signal, kernel, 1)
    same_out:i[]  = csntoarray(same)
    same_n:i      = csnsize(same)
    prints("same  : n = %d : %g %g %g %g %g\n", same_n, same_out[0], same_out[1], same_out[2], same_out[3], same_out[4])

    valid:CsnArr  = csnconvolve1d(signal, kernel, 2)
    valid_out:i[] = csntoarray(valid)
    valid_n:i     = csnsize(valid)
    prints("valid : n = %d : %g %g %g\n", valid_n, valid_out[0], valid_out[1], valid_out[2])

    ; along an axis of a matrix, every lane is convolved on its own
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    ones:CsnArr   = csnfromarray(array(1, 1))

    cols:CsnArr   = csnconvolve1d(mat, ones, 0, 0)
    csnprint cols

    rows:CsnArr   = csnconvolve1d(mat, ones, 0, 1)
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

* [csncorrelate1d](csncorrelate1d.md)
* [csnconvolve](csnconvolve.md)
* [csnhanning](csnhanning.md)
* [csnrfft](csnrfft.md)

## Credits

Pasquale Mainolfi, 2026
