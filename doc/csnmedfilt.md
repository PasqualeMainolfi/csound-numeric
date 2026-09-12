# csnmedfilt

## Abstract

N-dimensional median filter over a box, with the edges padded with zeros.

## Description

`csnmedfilt` replaces every element by the median of a box centred on it, one
size per axis, and counts what falls outside the array as zero. It is
`scipy.signal.medfilt` on an array of any rank, and for a 2-D source with a 2-D
kernel `scipy.signal.medfilt2d`.

The box is given in one of two ways: a single size, used on every axis, or one
size per axis. A size of `1` on an axis leaves that axis unfiltered, so
`fillarray(3, 1)` filters down the columns of a matrix and `fillarray(1, 3)`
across the rows. Every size must be odd — an even window has no centre to sit on
— and the array of sizes must hold exactly one entry per dimension of the
source.

Zero padding is what separates this from a windowed statistic such as
[csnmovmedian](csnmovmedian.md), and on a small array it dominates: a 3x3 box on
a 3x4 matrix has at least three of its nine cells outside the array everywhere,
and five of them at a corner, so the border comes back at or near zero. That is
scipy's answer too. Where a line rather than a box is wanted,
[csnmedfilt1d](csnmedfilt1d.md) filters along one axis and leaves the others
alone.

A NaN anywhere in the box makes that result NaN, as `np.median` does.

Real only: ordering has no meaning over the complex field.

The kernel is init-time on every overload, the k-rate form included. It fixes
the size of the working buffer, and settling that at init is what lets a
performance pass run without allocating. For the same reason the rank of the
source is fixed at init: a source that changes rank during performance is
refused by name, since the box was shaped for the rank it had.

Two forms share the name. The one with an output publishes a new handle and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing. In place the opcode filters against a private copy of the
source, which it must: the box reads neighbours on every axis, including cells
it has already rewritten. That copy is reserved at init from the array's
capacity, so a source that stays within it costs no allocation during
performance; a source that outgrows it is refused on a
[real-time path](csnrtlock.md).

At k-rate the in-place form applies one pass per external write: after it has
filtered, the array holds its own result and nothing has changed, so the next
pass does nothing. A zero trigger leaves the array untouched.

## Syntax

```csound
handle:CsnArr = csnmedfilt(source:CsnArr, kernel:i)
handle:CsnArr = csnmedfilt(source:CsnArr, kernel:i, trig:k)
handle:CsnArr = csnmedfilt(source:CsnArr, kernel:i[])
handle:CsnArr = csnmedfilt(source:CsnArr, kernel:i[], trig:k)
csnmedfilt(source:CsnArr, kernel:i)
csnmedfilt(source:CsnArr, kernel:i, trig:k)
csnmedfilt(source:CsnArr, kernel:i[])
csnmedfilt(source:CsnArr, kernel:i[], trig:k)
```

## Arguments

* `source:CsnArr`: the array to filter.
* `kernel:i`: one size for every axis; must be odd. Init-time on every overload.
* `kernel:i[]`: one size per axis, as many entries as the source has dimensions; each must be odd, and `1` leaves its axis unfiltered. Init-time on every overload.
* `trig:k` (optional, default `1`): k-rate trigger. In the output form a zero trigger republishes the previous result; in place it leaves the source untouched.

## Output

* `handle:CsnArr`: the filtered array, with the shape of the source. Omit it for the in-place form.

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
; csnmedfilt.csd
;
; The N-D median filter: a box centred on every element, zero outside the array.
; One size filters every axis alike; one size per axis filters only the axes you
; give a size greater than one, which is how a column or a row filter is asked
; for.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]       = fillarray(3, 4)
    flat:CsnArr     = csnfromarray(array(1, 9, 2, 3, 4, 1, 8, 2, 3, 5, 1, 7))
    source:CsnArr   = csnreshape(flat, shape)
    csnprint source

    ; a 3x3 box: on a matrix this small most windows are half padding, and the
    ; border comes back close to zero
    square:CsnArr   = csnmedfilt(source, 3)
    csnprint square

    ; one size per axis: 3 down the columns, 1 across, so the rows are untouched
    kcol:i[]        = fillarray(3, 1)
    columns:CsnArr  = csnmedfilt(source, kcol)
    csnprint columns

    ; and the transpose of that, a filter along the rows only
    krow:i[]        = fillarray(1, 3)
    rows:CsnArr     = csnmedfilt(source, krow)
    csnprint rows

    ; in place, no new handle: the source is rewritten
    copy:CsnArr     = csncopy(source)
    csnmedfilt(copy, 3)
    csnprint copy
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnmedfilt1d](csnmedfilt1d.md)
* [csnmovmedian](csnmovmedian.md)
* [csnconvolve](csnconvolve.md)
* [csnrtlock](csnrtlock.md)

## Credits

Pasquale Mainolfi, 2026
