# csnhexstrtoarr

## Abstract

The hex digits of one string as a string array, one digit each.

## Description

`csnhexstrtoarr` splits a string of hex digits into a one-dimensional
`:CsnStr;`, one element for each digit: `csnhexstrtoarr("0x7fA")` gives
`['7' 'f' 'A']`, the leftmost digit first and each as written, upper or
lower case. An optional `0x` (or `0X`) prefix is skipped; anything else that
is not a hex digit, a sign, a blank or an `_` included, is refused, the
message naming the string. `""` gives an empty array, a bare `0x` is refused.
[csnfromhex](csnfromhex.md) reads the array elementwise, so each digit gives
its value, 0 to 15.

A string has no rate, so the k forms take a trigger, and compute at init too:
an `S` that is still empty there (`chnget`, OSC) gives an empty array, and
the first pass that sees the digits fills it. The output keeps a copy of the
string it was computed from, so a pass that runs computes again only when the
string changed or something else wrote into the array; the array follows the
number of digits, and on a [real-time path](csnrtlock.md) a pass that needs
more room than the init pass gave it stops with a performance error.

## Syntax

```csound
out:CsnStr = csnhexstrtoarr(source:S)
out:CsnStr = csnhexstrtoarr(source:S, trig:k)
```

## Arguments

* `source:S`: hex digits, an optional `0x` prefix.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger keeps the previous result.

## Output

* `out:CsnStr`: a 1-D string array, one hex digit per element.

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
; csnhexstrtoarr.csd
;
; A pattern in hex beat notation, one digit for every four steps: the digits,
; and each one back as its four steps.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    digits:CsnStr  = csnhexstrtoarr("0x8A2F")
    csnprint(digits)
    beats:CsnArr   = csnfromhex(digits)
    steps:CsnStr   = csntobinary(beats, 4)
    csnprint(steps)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
CsnStr(shape=(4,), dtype=str)
['8' 'A' '2' 'F']
CsnStr(shape=(4,), dtype=str)
['1000' '1010' '0010' '1111']
```

## See also

* [csnbinstrtoarr](csnbinstrtoarr.md)
* [csnfromhex](csnfromhex.md)
* [csntohex](csntohex.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
