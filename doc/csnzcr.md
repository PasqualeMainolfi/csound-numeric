# csnzcr

## Abstract

Zero crossing rate, of a whole signal or along one axis.

## Description

`csnzcr` counts how often a signal changes sign and divides by its length. A
noisy or bright signal crosses often and a low or tonal one rarely, which makes
the rate a cheap measure of noisiness and a rough one of pitch.

It follows `librosa.feature.zero_crossing_rate` with its defaults: a value whose
magnitude is at most `1e-10` counts as `0`, and `0` counts as positive, so
`[1, 0, 1]` has no crossing and `[-1, 0, -1]` has two. The first element is never
a crossing by itself. The rate is the number of crossings over the number of
elements, `N`, not `N - 1`.

On a 1-D source the result is a number. On a source of two or more dimensions the
rate is taken along one axis, which disappears from the result: frames from
[csnframe](csnframe.md), of shape `(frames, length)`, give one rate per frame
along the last axis, as `librosa.feature.zero_crossing_rate(y, frame_length,
hop_length, center=False)[0]` does. The output decides the form: assign the result
to an `i` or `k` variable for the number, to a `CsnArr` for the array.

Real only; a complex source is refused. An empty source gives `0`, so a k form
whose source nothing has computed at init starts at `0` and computes on its first
pass.

The k-rate forms take a trigger and compute again when their source changes; a
zero trigger republishes the previous result. The array form then requires the
axis, so the number of arguments tells the forms apart.

## Syntax

```csound
value:i = csnzcr(source:CsnArr)
value:k = csnzcr(source:CsnArr, trig:k)
out:CsnArr = csnzcr(source:CsnArr [, axis:i])
out:CsnArr = csnzcr(source:CsnArr, trig:k, axis:i)
```

## Arguments

* `source:CsnArr`: a real array, 1-D for the number output, of two or more
  dimensions for the array output.
* `trig:k`: k-rate trigger. Zero republishes the previous result; a non-zero value
  computes again when the source has changed.
* `axis:i`: the axis to count along; negative values count from the end. Optional
  in the i-time form, where it defaults to `-1`, the last axis; required in the
  k-rate form.

## Output

* `value:i / value:k`: the rate of the whole 1-D source, from `0` to `(N - 1) / N`.
* `out:CsnArr`: one rate per position of the other axes.

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
; csnzcr.csd
;
; csnzcr counts the sign changes of a signal over its length: one number for a
; 1-D source, one value per frame for a framed one. The k-rate overloads take a
; trigger, then the axis for the array output.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    x:CsnArr = csnfromarray(fillarray(1, -1, 0, 2, -3, -1e-12, 4, 0, -2, 5, 6, -7))

    rate:i = csnzcr(x)
    prints("whole signal: %.4f\n", rate)

    frames:CsnArr = csnframe(x, 4, 2)
    per_frame:CsnArr = csnzcr(frames)
    prints("per frame:\n")
    csnprint(per_frame)

    kTrig init 1
    kOnce init 1
    live:CsnArr = csnzcr(frames, kTrig, -1)
    prints("k-rate, per frame:\n")
    csnprint(live, kOnce)
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
whole signal: 0.5833
per frame:
CsnArr(shape=(5,), dtype=float64)
[0.5 0.5 0.25 0.5 0.5]
k-rate, per frame:
CsnArr(shape=(5,), dtype=float64)
[0.5 0.5 0.25 0.5 0.5]
```

## See also

* [csnframe](csnframe.md)
* [csnamdf](csnamdf.md)
* [csnrms](csnrms.md)

## Credits

Pasquale Mainolfi, 2026
