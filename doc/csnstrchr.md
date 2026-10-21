# csnstrchr

## Abstract

The one-character string of an ASCII code, as Python's `chr` on ASCII.

## Description

`csnstrchr` returns the string of one byte whose ASCII code is `code`:
`csnstrchr(65)` gives `"A"`. The code is a whole number from 1 to 127;
anything else is refused, `0` included, since a NUL byte would end the string
at once. Unicode code points from 128 up, which Python's `chr` takes, are not
there: a string here is bytes, and the ASCII range is the one where a byte and
a character are the same.

[csnstrord](csnstrord.md) goes the other way, and
[csnstrfromascii](csnstrfromascii.md) turns a whole array of codes into a
string.

A k `code` selects the k form, which runs on every pass, its trigger optional
(default `1`); it computes at init when the code is known there.

## Syntax

```csound
out:S = csnstrchr(code:i)
out:S = csnstrchr(code:k [, trig:k])
```

## Arguments

* `code:i / code:k`: an ASCII code, a whole number from 1 to 127.
* `trig:k`: k-rate trigger, optional (default `1`). A zero trigger keeps the previous string.

## Output

* `out:S`: the one-byte string.

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
; csnstrchr.csd
;
; The letters of a scale built from their ASCII codes, one string each.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    first:S = csnstrchr(67)
    last:S  = csnstrchr(66)
    prints("%s to %s\n", first, last)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
C to B
```

## See also

* [csnstrord](csnstrord.md)
* [csnstrfromascii](csnstrfromascii.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
