# csnstraslist

## Abstract

The characters of one string as a string array, as Python's `list(s)`.

## Description

`csnstraslist` splits one `S` into a one-dimensional `:CsnStr;`, one
element for each character: `csnstraslist("abc")` gives `['a' 'b' 'c']`. The
string is read as UTF-8 and split by code point, as Python does, so
`csnstraslist("Ré♭")` gives `['R' 'é' '♭']`, each element holding the bytes of
its character; a string that is not UTF-8 is refused, the message naming the
byte where it stops being so. `""` gives an empty array. For the bytes or the
code points as numbers, use [csnstrtoascii](csnstrtoascii.md) or
[csnstrtocodepoints](csnstrtocodepoints.md); [csnstrjoin](csnstrjoin.md) with
`""` gives the string back.

A string has no rate, so the k form takes a trigger, and computes at init
too: an `S` that is still empty there (`chnget`, OSC) gives an empty array,
and the first pass that sees the text fills it. The output keeps a copy of
the string it was computed from, so a pass that runs computes again only
when the string changed or something else wrote into the array; the array
follows the number of characters, and on a [real-time path](csnrtlock.md) a
pass that needs more room than the init pass gave it stops with a
performance error.

## Syntax

```csound
out:CsnStr = csnstraslist(source:S)
out:CsnStr = csnstraslist(source:S, trig:k)
```

## Arguments

* `source:S`: UTF-8 text.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger keeps the previous result.

## Output

* `out:CsnStr`: a 1-D string array, one character per element.

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
; csnstraslist.csd
;
; A melody written as note letters, one per step, and a title whose accented
; letters stay whole.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    melody:CsnStr  = csnstraslist("CEGECEGA")
    csnprint(melody)
    title:CsnStr   = csnstraslist("Ré♭")
    csnprint(title)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
CsnStr(shape=(8,), dtype=str)
['C' 'E' 'G' 'E' 'C' 'E' 'G' 'A']
CsnStr(shape=(3,), dtype=str)
['R' 'é' '♭']
```

## See also

* [csnlistasstr](csnlistasstr.md)
* [csnstrjoin](csnstrjoin.md)
* [csnstrsplit](csnstrsplit.md)
* [csnstrtocodepoints](csnstrtocodepoints.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
