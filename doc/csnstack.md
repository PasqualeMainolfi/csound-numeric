# csnstack

## Abstract

Stack two or more equal-shaped arrays along a new axis.

## Description

`csnstack` inserts one new axis into the result and places each input array at
one position along it. The new axis has one entry per input. For example,
stacking three vectors of shape `(2,)` on axis 0 produces shape `(3, 2)`;
stacking them on axis 1 produces shape `(2, 3)`.

All inputs must have exactly the same shape, number of dimensions and element
type. Real and complex arrays are supported, but they cannot be mixed in the
same call. At least two input arrays are required. The axis is zero-based and
may range from `0` through the source dimension, inclusive, because it selects
where the new dimension is inserted.

In the k-rate form, the trigger and axis precede the variadic input list. The
number and identity of the input handles are fixed at init, while their data and
shape may change at performance time as long as every input continues to agree.
A zero trigger keeps the previous result; a non-zero trigger observes changed
inputs or axis and recomputes when needed.

The k-rate form determines its final layout on its first non-zero performance
pass and may allocate again when the input layout or axis changes. Such an
allocation is rejected if an input has placed the result on a real-time-locked
path.

## Syntax

```csound
result:CsnArr = csnstack(axis:i, first:CsnArr, second:CsnArr, ...)
result:CsnArr = csnstack(trig:k, axis:k, first:CsnArr, second:CsnArr, ...)
```

## Arguments

* `trig:k`: required k-rate trigger. Zero keeps the previous result; a non-zero
  value allows the operation to publish an updated stack.
* `axis:i / axis:k`: position of the new axis, in the inclusive range
  `[0, source dimensions]`.
* `first:CsnArr, second:CsnArr, ...`: two or more arrays with identical shape,
  dimensionality and element type.

## Output

* `result:CsnArr`: array with one additional dimension. Its extent on `axis` is
  the number of input arrays.

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
; csnstack.csd
;
; csnstack inserts a new axis and places each equal-shaped input at one position
; along it. The k-rate overload takes its trigger before the axis.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr = csnfromarray(array(1, 2))
    b:CsnArr = csnfromarray(array(3, 4))
    c:CsnArr = csnfromarray(array(5, 6))

    by_rows:CsnArr = csnstack(0, a, b, c)
    prints("axis 0:\n")
    csnprint(by_rows)

    kTrig init 1
    kAxis init 1
    kOnce init 1
    by_columns:CsnArr = csnstack(kTrig, kAxis, a, b, c)
    prints("axis 1:\n")
    csnprint(by_columns, kOnce)
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
axis 0:
CsnArr(shape=(3, 2), dtype=float64)
[[1 2]
 [3 4]
 [5 6]]
axis 1:
CsnArr(shape=(2, 3), dtype=float64)
[[1 3 5]
 [2 4 6]]
```

## See also

* [csnconcat](csnconcat.md)
* [csninsert](csninsert.md)
* [csnreshape](csnreshape.md)

## Credits

Pasquale Mainolfi, 2026
