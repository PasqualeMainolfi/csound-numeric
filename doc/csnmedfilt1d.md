# csnmedfilt1d

## Abstract

Median filter along one axis, with the edges padded with zeros.

## Description

`csnmedfilt1d` replaces every element by the median of a window of `kernel`
elements centred on it, and counts what falls outside the array as zero. It is
`scipy.signal.medfilt` on a vector, and on one axis of a matrix.

That padding is the only difference from [csnmovmedian](csnmovmedian.md), and it
is not a small one. Near the ends the window here is still full, half of it
zeros, so the result is pulled towards zero; `csnmovmedian` shortens the window
instead and invents nothing. On `1 100 2 3 4` with a window of 3 this opcode
answers `1 2 3 3 3` while `csnmovmedian` answers `50.5 2 3 3 3.5`: the spike
survives at the edge when the window is only two elements long. Pick this one to
match scipy, the other to keep the ends honest.

`kernel` must be odd — an even window has no centre to sit on — and may be `1`,
which filters nothing and returns the array as it stands. A NaN anywhere in the
window makes that result NaN, as `np.median` does.

The filter slides a sorted copy of the window along the array: each step removes
the element that leaves and inserts the one that arrives, instead of sorting
every window again. It is the same core [csnmovmedian](csnmovmedian.md) runs on.

With no axis the array is read flat and the result is one-dimensional, whatever
the shape of the source. Given an axis each line along it is filtered on its own
and the shape is kept.

For a window that spans several axes at once — a box rather than a line — use
[csnmedfilt](csnmedfilt.md).

Real only: ordering has no meaning over the complex field.

`kernel` is init-time on every overload, the k-rate form included. Its value
fixes the size of the working buffer, and settling that at init is what lets a
performance pass run without allocating. The axis may change at k-rate.

Two forms share the name. The one with an output publishes a new handle and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing. In place the source is not copied: the sliding window keeps
every value until it leaves, so a value already rewritten never re-enters a
later window.

At k-rate the in-place form applies one pass per external write: after it has
filtered, the array holds its own result and nothing has changed, so the next
pass does nothing. A zero trigger leaves the array untouched.

## Syntax

```csound
handle:CsnArr = csnmedfilt1d(source:CsnArr, kernel:i)
handle:CsnArr = csnmedfilt1d(source:CsnArr, kernel:i, axis:i)
handle:CsnArr = csnmedfilt1d(source:CsnArr, kernel:i, axis:k)
handle:CsnArr = csnmedfilt1d(source:CsnArr, kernel:i, axis:k, trig:k)
csnmedfilt1d(source:CsnArr, kernel:i)
csnmedfilt1d(source:CsnArr, kernel:i, axis:i)
csnmedfilt1d(source:CsnArr, kernel:i, axis:k)
csnmedfilt1d(source:CsnArr, kernel:i, axis:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to filter.
* `kernel:i`: the window length, in elements; must be odd. Init-time on every overload.
* `axis:i / axis:k` (optional, default `-1`): the axis to filter along; `-1` reads the array flat.
* `trig:k` (optional, default `1`): k-rate trigger. In the output form a zero trigger republishes the previous result; in place it leaves the source untouched.

## Output

* `handle:CsnArr`: the filtered array — the shape of the source along an axis, one-dimensional when the array is read flat. Omit it for the in-place form.

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
; csnmedfilt1d.csd
;
; A median filter with the edges scipy uses: what falls outside the array counts
; as zero, so the window is always full and the ends are pulled towards zero.
; csnmovmedian answers the same filter with a shorter window near the ends.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr      = csnfromarray(array(1, 100, 2, 3, 4))

    filtered:CsnArr  = csnmedfilt1d(data, 3)
    filtered_out:i[] = csntoarray(filtered)
    prints("kernel 3        : %g %g %g %g %g\n", filtered_out[0], filtered_out[1], filtered_out[2], filtered_out[3], filtered_out[4])

    ; a wider kernel: the spike is already gone, the zero padding reaches further
    wide:CsnArr      = csnmedfilt1d(data, 5)
    wide_out:i[]     = csntoarray(wide)
    prints("kernel 5        : %g %g %g %g %g\n", wide_out[0], wide_out[1], wide_out[2], wide_out[3], wide_out[4])

    ; the same window without the padding: at position 0 the window is just
    ; [1, 100], whose median is 50.5, and the spike survives
    moving:CsnArr    = csnmovmedian(data, 3)
    moving_out:i[]   = csntoarray(moving)
    prints("movmedian 3     : %g %g %g %g %g\n", moving_out[0], moving_out[1], moving_out[2], moving_out[3], moving_out[4])

    ; along one axis of a matrix: each row is filtered on its own
    shape:i[]        = fillarray(2, 5)
    flat:CsnArr      = csnfromarray(array(1, 5, 2, 4, 3, 5, 4, 3, 2, 1))
    matrix:CsnArr    = csnreshape(flat, shape)
    rows:CsnArr      = csnmedfilt1d(matrix, 3, 1)
    csnprint rows

    ; in place, no new handle: the source is rewritten
    copy:CsnArr      = csncopy(data)
    csnmedfilt1d(copy, 3)
    copy_out:i[]     = csntoarray(copy)
    prints("in place        : %g %g %g %g %g\n", copy_out[0], copy_out[1], copy_out[2], copy_out[3], copy_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnmedfilt](csnmedfilt.md)
* [csnmovmedian](csnmovmedian.md)
* [csnmedian](csnmedian.md)
* [csncorrelate1d](csncorrelate1d.md)

## Credits

Pasquale Mainolfi, 2026
