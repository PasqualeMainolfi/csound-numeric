# csnstrfromcodepoints

## Abstract

A UTF-8 string from an array of code points, as Python's `''.join(map(chr, points))`.

## Description

`csnstrfromcodepoints` returns the UTF-8 string of the code points of
`source`, in order: `[233 8364]` gives `"é€"`, and an empty array gives `""`.
The source is a real one-dimensional `:CsnArr;` of whole numbers from 1 to
1114111 (U+10FFFF), the surrogates 55296 to 57343 left out since UTF-8 cannot
write them; a value out of that range, a complex array or one of more
dimensions is refused, the message naming the first bad code point and its
index. UTF-8 is read as Python's strict decoder reads it: no overlong forms,
no surrogates (U+D800 to U+DFFF), nothing past U+10FFFF, and a NUL byte is
never written, since it would end the string at once.

[csnstrtocodepoints](csnstrtocodepoints.md) goes the other way;
[csnstrchr](csnstrchr.md) makes one ASCII character, and
[csnstrdecode](csnstrdecode.md) takes bytes.

The k form takes a trigger and writes the string again when the array changed;
like `strcpyk`, its output takes the room it needs at perf.

## Syntax

```csound
out:S = csnstrfromcodepoints(source:CsnArr)
out:S = csnstrfromcodepoints(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: code points, a real 1-D array of whole numbers from 1 to 1114111 but the surrogates.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger keeps the previous string.

## Output

* `out:S`: the UTF-8 string.

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
; csnstrfromcodepoints.csd
;
; The music signs flat, natural and sharp written from their code points.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    points:CsnArr = csnfromarray(array(9837, 9838, 9839))
    signs:S       = csnstrfromcodepoints(points)
    prints("%s\n", signs)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
♭♮♯
```

## See also

* [csnstrtocodepoints](csnstrtocodepoints.md)
* [csnstrdecode](csnstrdecode.md)
* [csnstrchr](csnstrchr.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
