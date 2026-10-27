# csnframe

## Abstract

Cut an axis into overlapping frames.

## Description

`csnframe` slides a window of `frame` elements along `axis`, `hop` elements at a
time, and stacks the windows it visits. The framed axis is replaced by two: the
frame index, then the position inside the frame. A source of shape
`(..., n, ...)` gives `(..., 1 + (n - frame) // hop, frame, ...)`, so seven
samples with `frame` 3 and `hop` 2 give three frames, `[[0 1 2] [2 3 4] [4 5 6]]`.
Every other axis is kept as it is, which frames each row of a matrix on its own.

This is NumPy's `sliding_window_view(x, frame, axis)` taken every `hop`-th window,
with the window axis placed next to the framed one instead of at the end:

```python
w = np.lib.stride_tricks.sliding_window_view(x, frame, axis=axis)
w = w[(slice(None),) * axis + (slice(None, None, hop),)]
np.moveaxis(w, -1, axis + 1)
```

On the last axis, the default, that is just `sliding_window_view(x, frame)[..., ::hop, :]`.
`librosa.util.frame` puts the two axes the other way round, frame length first.

Only whole frames are taken: elements after the last one that fits are dropped,
as they are in NumPy, so pad the source first with [csnpad](csnpad.md) to keep
them. The result is a copy, not a view: changing it leaves the source alone.
A frame longer than the source along the axis is an error, as are a `frame` or
`hop` that is not a whole number of at least 1 and a source that already has the
maximum number of dimensions.

Real, complex and string arrays are framed alike. On a `:CsnStr;` every element of
the result holds a copy of its string, and an element that falls in two frames is
copied into both. See [string arrays](string-arrays.md).

The k-rate form takes a trigger and frames again when its source changes; a zero
trigger republishes the previous result. `frame` and `hop` stay i-rate, so the
result keeps its shape across passes and the form runs on a
[real-time path](csnrtlock.md); on a `:CsnStr;` it writes into the room each element
already holds and stops with a performance error at the first string that does not
fit. A source that nothing has computed at init, such as the result of a k-rate
*csnconcat* with a k axis, starts as an empty placeholder: the output starts empty
too, and the first pass frames it.

The k-rate form requires the axis, so the number of arguments tells the forms
apart: four at most is the i-time form, its fourth the axis, and five is the k-rate
form, its fourth the trigger. A constant trigger is fine there:
`csnframe(x, 3, 2, 1, -1)`.

## Syntax

```csound
result:CsnArr = csnframe(source:CsnArr, frame:i, hop:i [, axis:i])
result:CsnArr = csnframe(source:CsnArr, frame:i, hop:i, trig:k, axis:i)
result:CsnStr = csnframe(source:CsnStr, frame:i, hop:i [, axis:i])
result:CsnStr = csnframe(source:CsnStr, frame:i, hop:i, trig:k, axis:i)
```

## Arguments

* `source:CsnArr / source:CsnStr`: the array to frame, real, complex or string.
* `frame:i`: elements in each frame, a whole number from 1 up to the source's
  extent along `axis`.
* `hop:i`: elements between the starts of two consecutive frames, a whole number
  of at least 1. A hop larger than the frame skips the elements in between.
* `trig:k`: k-rate trigger. Zero republishes the previous result; a non-zero value
  frames again when the source has changed.
* `axis:i`: the axis to frame; negative values count from the end. Optional in
  the i-time form, where it defaults to `-1`, the last axis; required in the k-rate
  form.

## Output

* `result:CsnArr / result:CsnStr`: a new array with one more dimension than the
  source, of the source's element type.

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
; csnframe.csd
;
; csnframe cuts an axis into overlapping frames: frame 3, hop 2 over seven
; samples gives three frames. The k-rate overload takes a trigger and then the
; axis, which it requires, and frames again when its source changes.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    x:CsnArr = csnarange(0, 7, 1)

    frames:CsnArr = csnframe(x, 3, 2)
    prints("frames of a vector:\n")
    csnprint(frames)

    iShape[] = fillarray(2, 7)
    m:CsnArr = csnreshape(csnarange(0, 14, 1), iShape)
    rows:CsnArr = csnframe(m, 4, 3, -1)
    prints("each row framed:\n")
    csnprint(rows)

    kTrig init 1
    kOnce init 1
    kPass init 0
    x[0] = kPass
    live:CsnArr = csnframe(x, 3, 2, kTrig, -1)
    if kPass == 2 then
        prints("after two passes:\n")
        csnprint(live, kOnce)
        kOnce = 0
    endif
    kPass += 1
endin

</CsInstruments>
<CsScore>
i 1 0 0.01
</CsScore>
</CsoundSynthesizer>
```

The example prints:

```text
frames of a vector:
CsnArr(shape=(3, 3), dtype=float64)
[[0 1 2]
 [2 3 4]
 [4 5 6]]
each row framed:
CsnArr(shape=(2, 2, 4), dtype=float64)
[[[0 1 2 3]
  [3 4 5 6]]

 [[7 8 9 10]
  [10 11 12 13]]]
after two passes:
CsnArr(shape=(3, 3), dtype=float64)
[[2 1 2]
 [2 3 4]
 [4 5 6]]
```

## See also

* [csnzcr](csnzcr.md)
* [csnamdf](csnamdf.md)
* [csnstft](csnstft.md)
* [csnpad](csnpad.md)
* [csnstack](csnstack.md)
* [csngetslice](csngetslice.md)

## Credits

Pasquale Mainolfi, 2026
