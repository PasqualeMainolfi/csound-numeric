# csnstrsplit

## Abstract

Splits one string into an array of strings at a separator, as Python's `str.split`.

## Description

`csnstrsplit` cuts `source` at every occurrence of `sep` and returns the
pieces in order: `"a,b,c"` on `","` gives `["a", "b", "c"]`. Occurrences are
found from the left and never overlap, so `"aaa"` on `"aa"` gives `["", "a"]`
(see [csnstrrsplit](csnstrrsplit.md) for the other way). Two separators in a
row, or one at either end, give an empty piece, and a string without `sep`, or
shorter than it, gives itself alone: `""` gives `[""]`.

`maxsplit` (default `-1`; any negative value means no limit) stops after that
many cuts and leaves the rest of the string in the last piece: `"a,b,c"` with
`1` gives `["a", "b,c"]`.

`sep` must not be empty, as in Python; Python's split on runs of whitespace,
without a `sep`, is not there.

NumPy's `np.strings.split` gives a list per string in an object array; one
string is not ragged, so here a single `S` goes in and a one-dimensional
`:CsnStr;` comes out: the bridge from one string to an array, with
[csnstrjoin](csnstrjoin.md) the way back.

The k form takes an explicit trigger, as a string has no i and k forms, and
the call without one is the init-time form: the trigger comes after
`maxsplit`, read at init, as in `csnstrsplit(Sline, ",", -1, kTrig)`. It
splits again only when `source` or `sep` changed, and its init splits too. On
a [real-time path](csnrtlock.md) every piece is written into the slot the init
pass gave its place, so a pass runs while each piece fits, and a longer one
stops it with a performance error that names both sizes.

## Syntax

```csound
out:CsnStr = csnstrsplit(source:S, sep:S [, maxsplit:i])
out:CsnStr = csnstrsplit(source:S, sep:S, maxsplit:i, trig:k)
```

## Arguments

* `source:S`: the string to split.
* `sep:S`: the separator, not empty.
* `maxsplit:i`: the most cuts to make, from the left; negative (default `-1`) for all of them.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger keeps the previous pieces.

## Output

* `out:CsnStr`: the pieces, a one-dimensional array of at least one string.

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
; csnstrsplit.csd
;
; A line of sample names, split into its names, then split once only: the
; empty name between two commas keeps its place.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    line:S       = "kick.wav,snare.wav,,hat.wav"
    names:CsnStr = csnstrsplit(line, ",")
    csnprint(names)
    first:CsnStr = csnstrsplit(line, ",", 1)
    csnprint(first)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
CsnStr(shape=(4,), dtype=str)
['kick.wav' 'snare.wav' '' 'hat.wav']
CsnStr(shape=(2,), dtype=str)
['kick.wav' 'snare.wav,,hat.wav']
```

## See also

* [csnstrrsplit](csnstrrsplit.md)
* [csnstrpartition](csnstrpartition.md)
* [csnstrjoin](csnstrjoin.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
