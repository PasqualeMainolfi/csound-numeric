# csnstrtoascii

## Abstract

The bytes of a string as a real array, as Python's `list(s.encode())`.

## Description

`csnstrtoascii` returns the bytes of `source` as a one-dimensional real
`:CsnArr;`, each from 0 to 255: `csnstrtoascii("Hi")` gives `[72 105]`, and an
empty string an empty array. Every byte counts, so a character that UTF-8
writes in two bytes gives two numbers (`"é"` gives `[195 169]`).

[csnstrfromascii](csnstrfromascii.md) goes back, for ASCII codes. The bytes
are not checked: [csnstrencode](csnstrencode.md) gives the same numbers for a
UTF-8 string and refuses one that is not, and
[csnstrtocodepoints](csnstrtocodepoints.md) gives its characters.

An `S` has no rate, so the k form takes a trigger, and computes at init too.
The string has no version to compare, so every pass that runs writes the bytes
again; the array follows the string's length, and on a [real-time
path](csnrtlock.md) a pass whose string is longer than the room the init pass
gave it stops with a performance error.

## Syntax

```csound
out:CsnArr = csnstrtoascii(source:S)
out:CsnArr = csnstrtoascii(source:S, trig:k)
```

## Arguments

* `source:S`: the string to read.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger keeps the previous array.

## Output

* `out:CsnArr`: the bytes, a real 1-D array.

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
; csnstrtoascii.csd
;
; The bytes of a word, as numbers to drive a sequence.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    bytes:CsnArr = csnstrtoascii("Hi!")
    csnprint(bytes)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
CsnArr(shape=(3,), dtype=float64)
[72 105 33]
```

## See also

* [csnstrfromascii](csnstrfromascii.md)
* [csnstrencode](csnstrencode.md)
* [csnstrord](csnstrord.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
