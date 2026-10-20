# csnstradd

## Abstract

Elementwise join of strings, as `np.strings.add`, or of two strings as
Python's `a + b`.

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

On two `S` it returns an `S`, `a` followed by `b`: `csnstradd("kick", ".wav")`
gives `"kick.wav"`, and the output may be one of the operands,
`Sname = csnstradd(Sname, ".wav")`. Its k form takes a trigger, joins again
on every pass that runs, and computes at init too; like `strcpyk`, its output
takes the room it needs at perf, also on a real-time path.

The operators `+`, `+=`, `*` and `*=` also work on `S`: `S1 + S2`, `Sx +=
Sy`, `S * n` and `Sx *= n` compute what the named forms compute. Csound has
no arithmetic on strings, so these operators exist only while csnum is
loaded; they add to the language rather than change it, and an orchestra
that uses them needs csnum. Like the other operators they take no trigger and
run on every pass, as `strcatk` does; `Sx += Sx` doubles `Sx`.

## Syntax

```csound
out:CsnStr = csnstradd(a:CsnStr, b:CsnStr)
out:CsnStr = csnstradd(a:CsnStr, b:CsnStr, trig:k)
out:CsnStr = csnstradd(a:CsnStr, suffix:S)
out:CsnStr = csnstradd(a:CsnStr, suffix:S, trig:k)
out:CsnStr = csnstradd(prefix:S, a:CsnStr)
out:CsnStr = csnstradd(prefix:S, a:CsnStr, trig:k)
out:S = csnstradd(a:S, b:S)
out:S = csnstradd(a:S, b:S, trig:k)
out:S = a:S + b:S
a:S += b:S
```

## Arguments

* `a:CsnStr`: the strings joined.
* `b:CsnStr`: the strings joined after `a`'s; broadcast against `a`.
* `suffix:S`: a string joined after every element of `a`.
* `prefix:S`: a string joined before every element of `a`.
* `a:S`, `b:S`: two strings, for the `S` form.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger republishes the previous result.

## Output

* `out:CsnStr`: the joined strings, in the broadcast shape.
* `out:S`: `a` followed by `b`, for the `S` form.

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

    one:S         = "kit/" + "ride.wav"
    one          += "!"
    prints("%s\n", one)
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
kit/ride.wav!
```

## See also

* [csnstrmul](csnstrmul.md)
* [csnconcat](csnconcat.md)
* [csnadd](csnadd.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
