# csninterp

## Abstract

Map values through a breakpoint table, with five interpolation modes.

## Description

`csninterp` reads a breakpoint table given as two parallel arrays — `x`, the
sample positions, and `y`, the values there — and returns the value at an
arbitrary position between them.

Two forms share the name. Given a **single position** it returns a single number;
given an **array of positions** it publishes a handle holding one result per
position. The array form runs at performance time, so its result is empty until
the first k-pass.

`x` must be ascending. The interpolation `mode` selects how the gaps are filled:

| mode | meaning |
|------|---------|
| `0`  | linear |
| `1`  | nearest neighbour |
| `2`  | previous breakpoint (sample and hold) |
| `3`  | next breakpoint |
| `4`  | monotone cubic (PCHIP) — smooth, and never overshoots the data |

The `bounds` argument decides what happens outside `[x[0], x[n-1]]`:

| bounds | meaning |
|--------|---------|
| `0`    | error — a position outside the table stops the note |
| `1`    | clamp to the first or last value |
| `2`    | fill with the `fill` argument |
| `3`    | extrapolate from the two end segments |

Real only.

## Syntax

```csound
value:i = csninterp(x:k, xs:CsnArr, ys:CsnArr, mode:i, bounds:i)
value:i = csninterp(x:k, xs:CsnArr, ys:CsnArr, mode:i, bounds:i, fill:i)
value:k = csninterp(x:k, xs:CsnArr, ys:CsnArr, mode:i, bounds:i, fill:i, trig:k)
handle:CsnArr = csninterp(positions:CsnArr, xs:CsnArr, ys:CsnArr, mode:i, bounds:i)
handle:CsnArr = csninterp(positions:CsnArr, xs:CsnArr, ys:CsnArr, mode:i, bounds:i, fill:i)
handle:CsnArr = csninterp(positions:CsnArr, xs:CsnArr, ys:CsnArr, mode:i, bounds:i, fill:i, axis:k)
handle:CsnArr = csninterp(positions:CsnArr, xs:CsnArr, ys:CsnArr, mode:i, bounds:i, fill:i, axis:k, trig:k)
```

## Arguments

* `x:k`: a single position to look up.
* `positions:CsnArr`: an array of positions to look up.
* `xs:CsnArr`: the breakpoint positions, ascending.
* `ys:CsnArr`: the values at those positions; same length as `xs`.
* `mode:i`: `0` linear, `1` nearest, `2` previous, `3` next, `4` monotone cubic.
* `bounds:i`: `0` error, `1` clamp, `2` fill, `3` extrapolate.
* `fill:i` (optional, default `0`): the value used outside the table when `bounds` is `2`.
* `axis:k` (optional, default `-1`): the axis of `positions` to walk; `-1` reads it flat.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `value:i / value:k`: the interpolated value, for a single position.
* `handle:CsnArr`: one value per position, for the array form.

## Execution Time

* Init (single-position form)
* Performance (k-rate)

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csninterp.csd
;
; A breakpoint table given as two parallel arrays. The mode fills the gaps and
; the bounds argument decides what happens off the ends.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

cap@global:i[]      = fillarray(0)
curve@global:CsnArr = csnempty(cap)

instr 1
    xs:CsnArr  = csnfromarray(array(0, 1, 2, 3))
    ys:CsnArr  = csnfromarray(array(0, 10, 20, 30))

    ; the five modes, at the same position
    linear:i   = csninterp(1.5, xs, ys, 0, 1)
    nearest:i  = csninterp(1.5, xs, ys, 1, 1)
    previous:i = csninterp(1.5, xs, ys, 2, 1)
    next:i     = csninterp(1.5, xs, ys, 3, 1)
    cubic:i    = csninterp(1.5, xs, ys, 4, 1)
    prints("at 1.5: linear=%g nearest=%g previous=%g next=%g cubic=%g\n", linear, nearest, previous, next, cubic)

    ; and the boundary policies, off the end of the table
    clamped:i  = csninterp(9, xs, ys, 0, 1)
    filled:i   = csninterp(9, xs, ys, 0, 2, -1)
    extrap:i   = csninterp(9, xs, ys, 0, 3)
    prints("at 9:   clamp=%g fill=%g extrapolate=%g\n", clamped, filled, extrap)

    ; the array form runs at performance time
    positions:CsnArr = csnfromarray(array(0.5, 1.5, 2.5))
    curve = csninterp(positions, xs, ys, 0, 1)
endin

instr 2
    n:i        = csnsize(curve)
    out:i[]    = csntoarray(curve)
    prints("array form, n = %d : %g %g %g\n", n, out[0], out[1], out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0   0.5
i 2 0.2 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnresample](csnresample.md)
* [csnlinspace](csnlinspace.md)
* [csnfromftable](csnfromftable.md)

## Credits

Pasquale Mainolfi, 2026
