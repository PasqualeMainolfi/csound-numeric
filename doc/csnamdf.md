# csnamdf

## Abstract

Average magnitude difference function, lag by lag, along one axis.

## Description

`csnamdf` compares a signal with itself delayed by `tau` samples and averages how
far apart the two are:

```text
D(tau) = sum(|x[i] - x[i + tau]|, i = 0 .. N - tau - 1) / (N - tau),   tau = 0 .. maxlag
```

`D(0)` is always `0`. A periodic signal comes back to itself after one period, so
`D` dips towards `0` there and again at every multiple of it, which is how the AMDF
finds pitch: the period is the lag of the first deep dip after `0`, and the
frequency `sr / period`. Dividing by the `N - tau` terms there are, rather than by
`N`, keeps a long lag from looking better only because it sums fewer of them.

The result is the curve, not the pitch. Search it yourself over the lags that
matter, from `sr / fmax` to `sr / fmin`: `D(0)` would always win, and the dips at
twice and three times the period can be nearly as deep as the first, so prefer the
first dip below a threshold to the deepest one when octave errors matter. A slice
and [csnargmin](csnargmin.md) give the deepest dip in a range, as in the example.

The lags replace the axis: a source of shape `(..., N, ...)` gives
`(..., maxlag + 1, ...)`, so a 1-D signal gives a 1-D curve and the frames of
[csnframe](csnframe.md), `(frames, length)`, one curve per frame along the last
axis. `maxlag` must be less than `N`. It is i-rate in both forms, since it sets
the shape of the result: the k form keeps its shape across passes and runs on a
[real-time path](csnrtlock.md). Each lag costs `N - tau` operations, so the whole
curve about `N * maxlag`.

Real only; a complex source is refused. A k form whose source nothing has
computed at init starts empty, and its first pass computes.

The k-rate form takes a trigger and computes again when its source changes; a
zero trigger republishes the previous result. It requires the axis, so the
number of arguments tells the forms apart.

## Syntax

```csound
out:CsnArr = csnamdf(source:CsnArr, maxlag:i [, axis:i])
out:CsnArr = csnamdf(source:CsnArr, maxlag:i, trig:k, axis:i)
```

## Arguments

* `source:CsnArr`: a real array of any rank.
* `maxlag:i`: the largest lag, a whole number from `0` to the source's extent
  along `axis` minus 1.
* `trig:k`: k-rate trigger. Zero republishes the previous result; a non-zero value
  computes again when the source has changed.
* `axis:i`: the axis to compare along; negative values count from the end.
  Optional in the i-time form, where it defaults to `-1`, the last axis; required in
  the k-rate form.

## Output

* `out:CsnArr`: the source's shape with `axis` replaced by `maxlag + 1` lags.

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
; csnamdf.csd
;
; csnamdf gives the average magnitude difference of a signal with itself
; delayed by 0 to maxlag samples. A periodic signal dips to 0 at its period:
; here 4 samples. The period is the first dip after lag 0, found with
; csnargmin on the lags worth searching; the k-rate overload takes maxlag,
; a trigger, then the axis.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    x:CsnArr = csnfromarray(fillarray(0, 1, 0, -1, 0, 1, 0, -1, 0, 1, 0, -1))

    curve:CsnArr = csnamdf(x, 6)
    prints("D(tau), tau 0..6:\n")
    csnprint(curve)

    ; search lags 2..6: lag 0 is always 0
    search:CsnArr = curve[2:6]
    at:CsnArr = csnargmin(search)
    period:i = at[0][0] + 2
    prints("period: %d samples\n", period)

    iShape[] = fillarray(2, 6)
    rows:CsnArr = csnreshape(csnfromarray(fillarray(0, 1, 0, -1, 0, 1, 1, 2, 3, 4, 5, 6)), iShape)
    kTrig init 1
    kOnce init 1
    per_row:CsnArr = csnamdf(rows, 2, kTrig, -1)
    prints("k-rate, one curve per row:\n")
    csnprint(per_row, kOnce)
    kOnce = 0
endin

</CsInstruments>
<CsScore>
i 1 0 0.01
</CsScore>
</CsoundSynthesizer>
```

The example prints:

```text
D(tau), tau 0..6:
CsnArr(shape=(7,), dtype=float64)
[0 1 1 1 0 1 1]
period: 4 samples
k-rate, one curve per row:
CsnArr(shape=(2, 3), dtype=float64)
[[0 1 1]
 [0 1 2]]
```

## See also

* [csnframe](csnframe.md)
* [csnzcr](csnzcr.md)
* [csnargmin](csnargmin.md)
* [csncorrelate1d](csncorrelate1d.md)

## Credits

Pasquale Mainolfi, 2026
