# csnstrjoin

## Abstract

Joins every string of an array into one string, as Python's `sep.join`.

## Description

`csnstrjoin` joins the strings of a `:CsnStr;` into one `S`, with `sep`
between two of them: `["a", "b", "c"]` with `", "` gives `"a, b, c"`. An
array of any rank joins in row-major order, the order
[csnflatten](csnflatten.md) gives, so a 2x2 grid joins its first row, then
its second. An empty string keeps its place (`["a", "", "c"]` gives
`"a, , c"`), one string gives itself, and an empty array gives `""`.
`sep` may be empty, which joins the strings end to end.

NumPy has no join over an array; this is the reduction strings call for, and
the way back from an array to one string.

The k form takes an explicit trigger, as a string has no i and k forms, and
the call without one is the init-time form. It joins again only when the
source or `sep` changed. Its init joins too, and the joined string sizes the
buffer it keeps and the `S` output, so on a [real-time path](csnrtlock.md) a
pass runs while its string is no longer than the one at init, and a longer
one stops it with a performance error that names both sizes. Off such a path
the output grows as it needs, as `strcpyk` does.

## Syntax

```csound
out:S = csnstrjoin(source:CsnStr, sep:S)
out:S = csnstrjoin(source:CsnStr, sep:S, trig:k)
```

## Arguments

* `source:CsnStr`: the strings to join, of any rank.
* `sep:S`: the string put between two of them; may be empty.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger keeps the previous string.

## Output

* `out:S`: the joined string.

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
; csnstrjoin.csd
;
; A 2x2 grid of sample names joined into one line, in row-major order, then
; written out as a single list.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    files:S[]    = fillarray("kick.wav", "snare.wav", "hat.wav", "")
    names:CsnStr = csnfromarray(files)
    shape:i[]    = fillarray(2, 2)
    grid:CsnStr  = csnreshape(names, shape)
    line:S       = csnstrjoin(grid, ", ")
    prints("[%s]\n", line)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
[kick.wav, snare.wav, hat.wav, ]
```

## See also

* [csnstradd](csnstradd.md)
* [csnflatten](csnflatten.md)
* [csntoarray](csntoarray.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
