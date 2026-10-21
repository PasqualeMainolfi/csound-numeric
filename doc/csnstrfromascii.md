# csnstrfromascii

## Abstract

A string from an array of ASCII codes, as Python's `bytes(codes).decode()` on ASCII.

## Description

`csnstrfromascii` returns the string whose bytes are the codes of `source`, in
order: `[72 105]` gives `"Hi"`, and an empty array gives `""`. The source is a
real one-dimensional `:CsnArr;` of ASCII codes, whole numbers from 1 to 127,
as [csnstrchr](csnstrchr.md) takes them; a code out of that range, a complex
array or one of more dimensions is refused, the message naming the first bad
code and its index.

[csnstrtoascii](csnstrtoascii.md) goes the other way.

The k form takes a trigger and writes the string again when the array changed;
like `strcpyk`, its output takes the room it needs at perf.

## Syntax

```csound
out:S = csnstrfromascii(source:CsnArr)
out:S = csnstrfromascii(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: ASCII codes, a real 1-D array of whole numbers from 1 to 127.
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
; csnstrfromascii.csd
;
; A word spelled from its codes, shifted up by one letter.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    codes:CsnArr = csnfromarray(array(72, 65, 76))
    next:CsnArr  = codes + 1
    word:S       = csnstrfromascii(next)
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
IBM
```

## See also

* [csnstrtoascii](csnstrtoascii.md)
* [csnstrchr](csnstrchr.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
