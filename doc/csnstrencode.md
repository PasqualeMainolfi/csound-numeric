# csnstrencode

## Abstract

The UTF-8 bytes of a string as a real array, as Python's `list(s.encode())`.

## Description

`csnstrencode` returns the bytes of `source` as a one-dimensional real
`:CsnArr;`, each from 1 to 255, after checking that they are UTF-8:
`csnstrencode("é")` gives `[195 169]`, and an empty string an empty array. A
string that is not UTF-8 is refused, the message naming the first byte where
it stops being so. UTF-8 is read as Python's strict decoder reads it: no
overlong forms, no surrogates (U+D800 to U+DFFF), nothing past U+10FFFF, and a
NUL byte is never written, since it would end the string at once.

A Csound string is already bytes, so the array holds the same numbers
[csnstrtoascii](csnstrtoascii.md) gives; the difference is the check, which
`csnstrtoascii` does not make. [csnstrdecode](csnstrdecode.md) goes back, and
[csnstrtocodepoints](csnstrtocodepoints.md) gives the characters instead of
their bytes.

An `S` has no rate, so the k form takes a trigger, and computes at init too.
The string has no version to compare, so every pass that runs writes the bytes
again; on a [real-time path](csnrtlock.md) a pass whose string is longer than
the room the init pass gave it stops with a performance error.

## Syntax

```csound
out:CsnArr = csnstrencode(source:S)
out:CsnArr = csnstrencode(source:S, trig:k)
```

## Arguments

* `source:S`: a UTF-8 string.
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
; csnstrencode.csd
;
; The bytes of a note name with an accidental: the flat sign takes three.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    bytes:CsnArr = csnstrencode("B♭")
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
CsnArr(shape=(4,), dtype=float64)
[66 226 153 173]
```

## See also

* [csnstrdecode](csnstrdecode.md)
* [csnstrtocodepoints](csnstrtocodepoints.md)
* [csnstrtoascii](csnstrtoascii.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
