# csnperm

## Abstract

The orderings of a 1-D array, one per row.

## Description

`csnperm` lists the permutations of the elements of a 1-D real array: a source
of `n` elements gives a result of shape `(n!, n)`, each row one ordering of the
source. With a `count`, only the first `count` rows are made, so a long source
can give a few orderings without the `n!` of them all; a count past `n!` stops
at `n!`, and `0` gives no rows, shape `(0, n)`.

The rows are those of `np.array(list(itertools.permutations(x)))`, in another
order: Heap's algorithm, which reaches each ordering from the one before with a
single swap. The first row is the source as it is, the second its first two
elements swapped. Equal elements are not merged, so `[1, 1, 2]` still gives six
rows, two of each ordering, as `itertools` does. An empty source has one
ordering, the empty one: shape `(1, 0)`.

The count grows fast: 10 elements give 3,628,800 rows, about 290 MB, and every
ordering of 13 or more is refused, since the rows would not fit in a shape.
Pass a count to take the first ones of a longer source.

Real only; a complex or string array is refused, as is a source of more than
one dimension.

The k-rate form takes a trigger and lists again when its source changes; a zero
trigger republishes the previous result. `count` stays i-rate, so the result
keeps its shape while the source keeps its length, and the form runs on a
[real-time path](csnrtlock.md). A source that nothing has computed at init, such
as the result of a k-rate *csnconcat* with a k axis, starts the output empty, and
the first pass lists it. The k form requires the count, so the number of
arguments tells the forms apart.

## Syntax

```csound
out:CsnArr = csnperm(source:CsnArr [, count:i])
out:CsnArr = csnperm(source:CsnArr, count:i, trig:k)
```

## Arguments

* `source:CsnArr`: a 1-D real array.
* `count:i`: the number of permutations to make, a whole number of at least `0`,
  or `-1` for all of them. Optional in the i-time form, where it defaults to
  `-1`; required in the k-rate form.
* `trig:k`: k-rate trigger. Zero republishes the previous result; a non-zero value
  lists again when the source has changed.

## Output

* `out:CsnArr`: a real array of shape `(rows, n)`, `rows` being `n!` or the
  smaller `count`.

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
; csnperm.csd
;
; csnperm lists the orderings of a 1-D array, one per row: all n! of them, or
; only the first few. Here the orderings of a three-note motif, then the first
; four of a four-note chord. The k-rate overload takes the count and a trigger,
; and lists again when its source changes.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    motif:CsnArr = csnfromarray(fillarray(60, 64, 67))
    orders:CsnArr = csnperm(motif)
    prints("every ordering of the motif:\n")
    csnprint(orders)

    chord:CsnArr = csnfromarray(fillarray(48, 55, 64, 70))
    first:CsnArr = csnperm(chord, 4)
    prints("the first four of 24:\n")
    csnprint(first)

    kTrig init 1
    kPass init 0
    kOnce init 1
    motif[0] = 60 + kPass
    live:CsnArr = csnperm(motif, 2, kTrig)
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
every ordering of the motif:
CsnArr(shape=(6, 3), dtype=float64)
[[60 64 67]
 [64 60 67]
 [67 60 64]
 [60 67 64]
 [64 67 60]
 [67 64 60]]
the first four of 24:
CsnArr(shape=(4, 4), dtype=float64)
[[48 55 64 70]
 [55 48 64 70]
 [64 48 55 70]
 [48 64 55 70]]
after two passes:
CsnArr(shape=(2, 3), dtype=float64)
[[62 64 67]
 [64 62 67]]
```

## See also

* [csnshuffle](csnshuffle.md)
* [csnsort](csnsort.md)
* [csntranspose](csntranspose.md)

## Credits

Pasquale Mainolfi, 2026
