# csnstradd

## Abstract

Elementwise join of strings, as `np.strings.add`.

## Description

`csnstradd` joins its operands element by element: each string of the result
is the string from the first operand followed by the one from the second. Two
`:CsnStr;` arrays are broadcast against each other NumPy-style, aligned from
the last axis, each pair of extents matching or 1, so a column and a row give
a grid. One operand may be a single `S`, which is joined after every element
(`csnstradd(arr, "x")`) or before it (`csnstradd("x", arr)`), since joining
strings is not commutative.

The result is a `:CsnStr;` holding its own strings. It may be one of the
operands, `names = csnstradd(names, ".wav")`: each element is read before it
is written, and the k form then joins again on every pass that runs.

A string argument has no i and k forms, so the k forms with an `S` take an
explicit trigger, and the call without one is the init-time form. The k form
of two arrays runs again only when an operand changed.

Each element is written into the room it already holds. On a [real-time
path](csnrtlock.md) the init pass sizes the elements, so the k form runs as
long as every result fits that room; it checks every element first and stops
with a performance error, before anything is written, when one does not. See
[string arrays](string-arrays.md#real-time-paths).

## Syntax

```csound
out:CsnStr = csnstradd(a:CsnStr, b:CsnStr)
out:CsnStr = csnstradd(a:CsnStr, b:CsnStr, trig:k)
out:CsnStr = csnstradd(a:CsnStr, suffix:S)
out:CsnStr = csnstradd(a:CsnStr, suffix:S, trig:k)
out:CsnStr = csnstradd(prefix:S, a:CsnStr)
out:CsnStr = csnstradd(prefix:S, a:CsnStr, trig:k)
```

## Arguments

* `a:CsnStr`: the strings joined.
* `b:CsnStr`: the strings joined after `a`'s; broadcast against `a`.
* `suffix:S`: a string joined after every element of `a`.
* `prefix:S`: a string joined before every element of `a`.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger republishes the previous result.

## Output

* `out:CsnStr`: the joined strings, in the broadcast shape.

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
; csnstradd.csd
;
; File names built from a list of drums: a string after every element, a
; string before it, and a column against a row, broadcast to a grid.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    names:S[]     = fillarray("kick", "snare", "hat")
    drums:CsnStr  = csnfromarray(names)
    files:CsnStr  = csnstradd(drums, ".wav")
    paths:CsnStr  = csnstradd("kit/", files)
    csnprint(paths)

    column:i[]    = fillarray(3, 1)
    rows:CsnStr   = csnreshape(drums, column)
    sides:S[]     = fillarray("_L", "_R")
    stereo:CsnStr = csnstradd(rows, csnfromarray(sides))
    csnprint(stereo)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
CsnStr(shape=(3,), dtype=str)
['kit/kick.wav' 'kit/snare.wav' 'kit/hat.wav']
CsnStr(shape=(3, 2), dtype=str)
[['kick_L' 'kick_R']
 ['snare_L' 'snare_R']
 ['hat_L' 'hat_R']]
```

## See also

* [csnstrmul](csnstrmul.md)
* [csnconcat](csnconcat.md)
* [csnadd](csnadd.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
