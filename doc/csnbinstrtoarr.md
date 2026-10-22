# csnbinstrtoarr

## Abstract

The binary digits of one string as a real array of 0 and 1.

## Description

`csnbinstrtoarr` splits a string of binary digits into a real
one-dimensional array, one element for each digit: `csnbinstrtoarr("0b1011")`
gives `[1 0 1 1]`, the leftmost digit first. An optional `0b` (or `0B`)
prefix is skipped; anything else that is not `0` or `1`, a sign, a blank or
an `_` included, is refused, the message naming the string. `""` gives an
empty array, a bare `0b` is refused. The digits are kept as written, leading
zeros too, so a step pattern keeps its length; for the number they make, use
[csnfrombinary](csnfrombinary.md).

A string has no rate, so the k forms take a trigger, and compute at init too:
an `S` that is still empty there (`chnget`, OSC) gives an empty array, and
the first pass that sees the digits fills it. The output keeps a copy of the
string it was computed from, so a pass that runs computes again only when the
string changed or something else wrote into the array; the array follows the
number of digits, and on a [real-time path](csnrtlock.md) a pass that needs
more room than the init pass gave it stops with a performance error.

## Syntax

```csound
out:CsnArr = csnbinstrtoarr(source:S)
out:CsnArr = csnbinstrtoarr(source:S, trig:k)
```

## Arguments

* `source:S`: binary digits, an optional `0b` prefix.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger keeps the previous result.

## Output

* `out:CsnArr`: a real 1-D array of 0 and 1, one element per digit.

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
; csnbinstrtoarr.csd
;
; A drum pattern written as binary digits, one step each, and the hits it
; holds.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    kick:CsnArr    = csnbinstrtoarr("0b1000100010001000")
    csnprint(kick)
    hits:i         = csnsum(kick)
    prints("kick hits %d of %d steps\n", hits, csnsize(kick))
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
CsnArr(shape=(16,), dtype=float64)
[1 0 0 0 1 0 0 0 1 0 0 0 1 0 0 0]
kick hits 4 of 16 steps
```

## See also

* [csnhexstrtoarr](csnhexstrtoarr.md)
* [csnfrombinary](csnfrombinary.md)
* [csntobinary](csntobinary.md)
* [csnstrtoascii](csnstrtoascii.md)

## Credits

Pasquale Mainolfi, 2026
