# csnstrrpartition

## Abstract

Splits one string at the last occurrence of a separator into head, separator and tail, as Python's `str.rpartition`.

## Description

`csnstrrpartition` finds the last occurrence of `sep` in `source` and returns
three strings: what comes before it, `sep` itself, and what comes after it:
`"kick.final.wav"` on `"."` gives `["kick.final", ".", "wav"]`. Without `sep`
the string comes last and the two others are empty: `["", "", "source"]`, so
the middle string tells whether `sep` was found. `sep` must not be empty, as
in Python.

NumPy's `np.strings.rpartition` gives three arrays; one string in gives one
array of three strings out here, with [csnstrjoin](csnstrjoin.md) (`sep` `""`)
the way back.

The k form takes an explicit trigger, as a string has no i and k forms, and
the call without one is the init-time form. It splits again only when `source`
or `sep` changed, and its init splits too. On a [real-time path](csnrtlock.md)
every piece is written into the slot the init pass gave its place, so a pass
runs while each piece fits, and a longer one stops it with a performance error
that names both sizes.

## Syntax

```csound
out:CsnStr = csnstrrpartition(source:S, sep:S)
out:CsnStr = csnstrrpartition(source:S, sep:S, trig:k)
```

## Arguments

* `source:S`: the string to split.
* `sep:S`: the separator, not empty.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger keeps the previous strings.

## Output

* `out:CsnStr`: three strings: the head, the separator (or `""`) and the tail.

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
; csnstrrpartition.csd
;
; A file name split at its last dot into name and extension; a name without
; a dot comes last, with the two others empty.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    file:CsnStr = csnstrrpartition("kick.final.wav", ".")
    csnprint(file)
    bare:CsnStr = csnstrrpartition("README", ".")
    csnprint(bare)
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
['kick.final' '.' 'wav']
CsnStr(shape=(3,), dtype=str)
['' '' 'README']
```

## See also

* [csnstrpartition](csnstrpartition.md)
* [csnstrrsplit](csnstrrsplit.md)
* [csnstrrfind](csnstrrfind.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
