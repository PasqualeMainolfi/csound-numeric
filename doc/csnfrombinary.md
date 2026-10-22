# csnfrombinary

## Abstract

Reads a string of binary digits as a number, as Python's `int(s, 2)`.

## Description

`csnfrombinary` reads a string of binary digits as the number it writes, as
Python's `int(s, 2)` does: `csnfrombinary("0b101")` gives 5. The string holds
`0` and `1`, after an optional `0b` or `0B` prefix, and nothing else: an empty
string or a prefix alone is refused, and so are a sign, a blank and an
underscore, which `int()` accepts. The number must stay below 2^64; above 2^53
it comes back as the nearest double, since Csound's numbers are doubles.
[csntobinary](csntobinary.md) writes the digits.

A `:CsnStr;` gives a real `CsnArr` of its shape; a string that is refused is
named with its index.

The `i` output takes the init-time form, a `k` output the k form, which reads
the string at perf, on every pass its trigger is non-zero. The array form
takes an explicit trigger, and the call without one is the init-time form; it
runs again only when the source changed, and on a [real-time
path](csnrtlock.md) it allocates nothing while the source keeps its shape.

## Syntax

```csound
out:i = csnfrombinary(source:S)
out:k = csnfrombinary(source:S [, trig:k])
out:CsnArr = csnfrombinary(source:CsnStr)
out:CsnArr = csnfrombinary(source:CsnStr, trig:k)
```

## Arguments

* `source:S`: a string of binary digits, with or without `0b`.
* `source:CsnStr`: an array of such strings.
* `trig:k`: k-rate trigger, optional in the scalar k form (default `1`), which selects the k form of the array one. A zero trigger keeps the previous result.

## Output

* `out:i`, `out:k`: the number.
* `out:CsnArr`: the number of every string of `source`, in its shape.

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
; csnfrombinary.csd
;
; Drum patterns written as steps, read back as the numbers that hold them.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    kick:i         = csnfrombinary("1000100010001000")
    snare:i        = csnfrombinary("0b0000100000001000")
    prints("kick %d, snare %d\n", kick, snare)
    lines:S[]      = fillarray("1010", "0110", "1111")
    patterns:CsnStr = csnfromarray(lines)
    values:CsnArr  = csnfrombinary(patterns)
    csnprint(values)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
kick 34952, snare 2056
CsnArr(shape=(3,), dtype=float64)
[10 6 15]
```

## See also

* [csntobinary](csntobinary.md)
* [csnfromhex](csnfromhex.md)
* [csnstrtonum](csnstrtonum.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
