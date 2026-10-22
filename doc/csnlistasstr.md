# csnlistasstr

## Abstract

The numbers of a real array as one string, joined by a separator, as Python's `sep.join(map(str, a))`.

## Description

`csnlistasstr` writes the numbers of a real one-dimensional array into one
`S`, separated by `sep`: `csnlistasstr(csnlinspace(0, 1.5, 4), ", ")` gives
`"0.0, 0.5, 1.0, 1.5"`. Each number is written as
[csnnumtostr](csnnumtostr.md) writes it, Python's `repr`: the fewest digits
that read back (`"0.1"`, not `"0.10000000000000001"`), at least one decimal
(`"1.0"`), an exponent outside 1e-4 to 1e16 (`"1e+16"`), and `"nan"`,
`"inf"` and `"-inf"`. `sep` may be `""`, and an empty array gives `""`. A
complex array, or one of more than one dimension, is refused. It is
`csnstrjoin(csnnumtostr(a), sep)` in one step, without the string array in
between.

The k form takes a trigger, and computes at init too; a pass that runs
computes again only when the array or `sep` changed. Like `strcpyk`, the `S`
output takes the room it needs at perf.

## Syntax

```csound
out:S = csnlistasstr(source:CsnArr, sep:S)
out:S = csnlistasstr(source:CsnArr, sep:S, trig:k)
```

## Arguments

* `source:CsnArr`: a real 1-D array.
* `sep:S`: the text between two numbers, `""` for none.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger keeps the previous result.

## Output

* `out:S`: the numbers, joined by `sep`.

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
; csnlistasstr.csd
;
; The partials of a harmonic tone written as one line, ready for a log or an
; OSC message.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ratios:CsnArr  = csnlinspace(1, 4, 4)
    partials:CsnArr = ratios * 110
    line:S         = csnlistasstr(partials, ", ")
    prints("partials %s\n", line)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
partials 110.0, 220.0, 330.0, 440.0
```

## See also

* [csnstraslist](csnstraslist.md)
* [csnnumtostr](csnnumtostr.md)
* [csnstrjoin](csnstrjoin.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
