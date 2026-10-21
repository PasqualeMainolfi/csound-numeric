# csnstrdecode

## Abstract

A string from an array of UTF-8 bytes, as Python's `bytes(b).decode()`.

## Description

`csnstrdecode` returns the string whose bytes are the values of `source`, in
order, after checking that they are UTF-8: `[195 169]` gives `"é"`, and an
empty array gives `""`. The source is a real one-dimensional `:CsnArr;` of
whole numbers from 1 to 255; a value out of that range, a complex array, one
of more dimensions, or bytes that are not UTF-8 are refused, the message
naming the first bad index. UTF-8 is read as Python's strict decoder reads it:
no overlong forms, no surrogates (U+D800 to U+DFFF), nothing past U+10FFFF,
and a NUL byte is never written, since it would end the string at once.

[csnstrencode](csnstrencode.md) goes the other way;
[csnstrfromascii](csnstrfromascii.md) takes ASCII codes only, and
[csnstrfromcodepoints](csnstrfromcodepoints.md) takes characters.

The k form takes a trigger and writes the string again when the array changed;
like `strcpyk`, its output takes the room it needs at perf.

## Syntax

```csound
out:S = csnstrdecode(source:CsnArr)
out:S = csnstrdecode(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: UTF-8 bytes, a real 1-D array of whole numbers from 1 to 255.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger keeps the previous string.

## Output

* `out:S`: the string.

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
; csnstrdecode.csd
;
; A word read back from its UTF-8 bytes, accent included.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    bytes:CsnArr = csnfromarray(array(99, 105, 116, 116, 195, 160))
    word:S       = csnstrdecode(bytes)
    prints("%s\n", word)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
città
```

## See also

* [csnstrencode](csnstrencode.md)
* [csnstrfromcodepoints](csnstrfromcodepoints.md)
* [csnstrfromascii](csnstrfromascii.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
