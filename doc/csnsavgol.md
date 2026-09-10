# csnsavgol

## Abstract

Coefficient matrix of a Savitzky-Golay filter, one row per derivative order.

## Description

`csnsavgol` builds the filter and hands it to you; applying it is
[csncorrelate1d](csncorrelate1d.md)'s job. A Savitzky-Golay filter fits a
polynomial of degree `order` to every `winsize`-sample window by least squares
and reads a value off the fit, which smooths a signal without the flattening a
moving average causes, and differentiates it without the noise amplification a
plain difference causes.

The result is a `(order + 1) x winsize` matrix. Row `d` is the filter for the
`d`-th derivative of the fitted polynomial: row 0 smooths, row 1 gives the
first derivative, and so on. Each row is already scaled by `d! / delta^d`, so
`delta` — the spacing between samples — comes out in the units you asked for
rather than per-sample.

The matrix is the pseudo-inverse of the Vandermonde design matrix,
`(A^T A)^-1 A^T` with `A[i][j] = (i - centre)^j`, which is how
`scipy.signal.savgol_coeffs` computes the same numbers.

### Applying it

Take a row with [csngetrow](csngetrow.md) and correlate:

```csound
coeffs:CsnArr = csnsavgol(7, 2, 1)
smooth:CsnArr = csngetrow(coeffs, 0)
fitted:CsnArr = csncorrelate1d(signal, smooth, 2)
```

**Correlate, not convolve.** The row comes out in natural order, from position
`-m` to `+m`, and that is the order correlation applies it in. Convolution
reverses the kernel, which is harmless for the even rows — they are symmetric,
so both give the same answer — and flips the sign of every odd row. On the
example below the first derivative comes out as `9 11 13 ...` correlated and
`-9 -11 -13 ...` convolved. Convolution is usable if you reverse the row first,
but there is no reason to.

`VALID` is the mode that matches what the filter means: it keeps only the
positions where the window is full, which is where the fit has all of its
points. `FULL` and `SAME` extend the signal with zeros, and a polynomial fit
against invented zeros is not a fit of anything.

The opcode is init-time only, and deliberately: `winsize`, `order` and `delta`
are all i-rate, so there is nothing about the matrix that could change during
performance. Compute it once and correlate with it as often as you like — the
k-rate form belongs to [csncorrelate1d](csncorrelate1d.md), which has one.

Real only. `winsize` must be odd and at least 3, `order` at least 0 and less
than `winsize`, `delta` greater than zero; each is refused by name.

## Syntax

```csound
handle:CsnArr = csnsavgol(winsize:i, order:i)
handle:CsnArr = csnsavgol(winsize:i, order:i, delta:i)
```

## Arguments

* `winsize:i`: the window length; must be odd and at least 3.
* `order:i`: the degree of the polynomial fitted to each window; at least 0 and less than `winsize`.
* `delta:i` (optional, default `1`): the spacing between samples, used to scale the derivative rows.

## Output

* `handle:CsnArr`: an `(order + 1) x winsize` matrix, row `d` being the filter for the `d`-th derivative.

## Execution Time

* Init

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsavgol.csd
;
; The coefficient matrix of a Savitzky-Golay filter, one row per derivative
; order, applied with csncorrelate1d. The test signal is a parabola, which a
; second-order fit reproduces exactly, so smoothing gives the signal back and
; the first derivative comes out as 2t + 3 with no error to speak of.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    coeffs:CsnArr = csnsavgol(7, 2, 1)
    csnprint coeffs

    smooth:CsnArr = csngetrow(coeffs, 0)
    slope:CsnArr  = csngetrow(coeffs, 1)

    t:CsnArr      = csnarange(0, 12, 1)
    signal:CsnArr = csnadd(csnmul(t, t), csnmul(t, 3))

    ; VALID keeps only the positions where the window is full, which is where
    ; the fit has all of its points
    fitted:CsnArr = csncorrelate1d(signal, smooth, 2)
    csnprint fitted

    derivative:CsnArr = csncorrelate1d(signal, slope, 2)
    csnprint derivative

    ; correlate applies the row as it stands; convolve reverses it, which flips
    ; the sign of every odd-order row - the even ones are symmetric and agree
    flipped:CsnArr = csnconvolve1d(signal, slope, 2)
    csnprint flipped
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
* [csngetrow](csngetrow.md)
* [csnconvolve1d](csnconvolve1d.md)
* [csnmovmean](csnmovmean.md)

## Credits

Pasquale Mainolfi, 2026
