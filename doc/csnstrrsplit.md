# csnstrrsplit

## Abstract

Splits one string into an array of strings at a separator, from the right, as Python's `str.rsplit`.

## Description

`csnstrrsplit` cuts `source` at the occurrences of `sep`, found from the
right, and returns the pieces in their order in the string. With every cut it
gives what [csnstrsplit](csnstrsplit.md) gives, except where occurrences could
overlap: `"aaa"` on `"aa"` gives `["a", ""]`. Two separators in a row, or one
at either end, give an empty piece, and a string without `sep`, or shorter
than it, gives itself alone.

`maxsplit` (default `-1`; any negative value means no limit) stops after that
many cuts and leaves the rest of the string in the first piece: `"a,b,c"` with
`1` gives `["a,b", "c"]`, the way to take the last field off a string.

`sep` must not be empty, as in Python; Python's split on runs of whitespace,
without a `sep`, is not there.

NumPy's `np.strings.rsplit` gives a list per string in an object array; one
string is not ragged, so here a single `S` goes in and a one-dimensional
`:CsnStr;` comes out: the bridge from one string to an array, with
[csnstrjoin](csnstrjoin.md) the way back.

The k form takes an explicit trigger, as a string has no i and k forms, and
the call without one is the init-time form: the trigger comes after
`maxsplit`, read at init, as in `csnstrrsplit(Spath, "/", 1, kTrig)`. It
splits again only when `source` or `sep` changed, and its init splits too. On
a [real-time path](csnrtlock.md) every piece is written into the slot the init
pass gave its place, so a pass runs while each piece fits, and a longer one
stops it with a performance error that names both sizes.

## Syntax

```csound
out:CsnStr = csnstrrsplit(source:S, sep:S [, maxsplit:i])
out:CsnStr = csnstrrsplit(source:S, sep:S, maxsplit:i, trig:k)
```

## Arguments

* `source:S`: the string to split.
* `sep:S`: the separator, not empty.
* `maxsplit:i`: the most cuts to make, from the right; negative (default `-1`) for all of them.
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
; csnstrrsplit.csd
;
; A sample path, split into its folders and file, then split once from the
; right, which leaves the folder and the file name.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    path:S        = "samples/drums/kick.wav"
    parts:CsnStr  = csnstrrsplit(path, "/")
    csnprint(parts)
    folder:CsnStr = csnstrrsplit(path, "/", 1)
    csnprint(folder)
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
['samples' 'drums' 'kick.wav']
CsnStr(shape=(2,), dtype=str)
['samples/drums' 'kick.wav']
```

## See also

* [csnstrsplit](csnstrsplit.md)
* [csnstrrpartition](csnstrrpartition.md)
* [csnstrjoin](csnstrjoin.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
