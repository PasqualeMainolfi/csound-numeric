# csnstrtocodepoints

## Abstract

The code points of a UTF-8 string as a real array, as Python's `[ord(c) for c in s]`.

## Description

`csnstrtocodepoints` reads `source` as UTF-8 and returns its characters' code
points as a one-dimensional real `:CsnArr;`, one element per character:
`csnstrtocodepoints("é€")` gives `[233 8364]`, and an empty string an empty
array. A string that is not UTF-8 is refused, the message naming the first
byte where it stops being so. UTF-8 is read as Python's strict decoder reads
it: no overlong forms, no surrogates (U+D800 to U+DFFF), nothing past
U+10FFFF, and a NUL byte is never written, since it would end the string at
once.

[csnstrfromcodepoints](csnstrfromcodepoints.md) goes back;
[csnstrencode](csnstrencode.md) gives the bytes instead, and
[csnstrlen](csnstrlen.md) counts bytes, so the size of this array is the
number of characters.

An `S` has no rate, so the k form takes a trigger, and computes at init too.
The string has no version to compare, so every pass that runs writes the code
points again; on a [real-time path](csnrtlock.md) a pass with more characters
than the init pass had stops with a performance error.

## Syntax

```csound
out:CsnArr = csnstrtocodepoints(source:S)
out:CsnArr = csnstrtocodepoints(source:S, trig:k)
```

## Arguments

* `source:S`: a UTF-8 string.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger keeps the previous array.

## Output

* `out:CsnArr`: the code points, a real 1-D array.

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
; csnstrtocodepoints.csd
;
; The characters of a label counted apart from its bytes.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    points:CsnArr = csnstrtocodepoints("Ré♯")
    csnprint(points)
    prints("%d characters, %d bytes\n", csnsize(points), csnstrlen("Ré♯"))
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
[82 233 9839]
3 characters, 6 bytes
```

## See also

* [csnstrfromcodepoints](csnstrfromcodepoints.md)
* [csnstrencode](csnstrencode.md)
* [csnstrlen](csnstrlen.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
