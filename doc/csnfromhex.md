# csnfromhex

## Abstract

Reads a string of hexadecimal digits as a number, as Python's `int(s, 16)`.

## Description

`csnfromhex` reads a string of hex digits as the number it writes, as Python's
`int(s, 16)` does: `csnfromhex("0xFF")` gives 255. The string holds `0` to `9`
and `a` to `f`, in either case, after an optional `0x` or `0X` prefix, and
nothing else: an empty string or a prefix alone is refused, and so are a sign,
a blank and an underscore, which `int()` accepts. The number must stay below
2^64; above 2^53 it comes back as the nearest double, since Csound's numbers
are doubles. [csntohex](csntohex.md) writes the digits.

A `:CsnStr;` gives a real `CsnArr` of its shape; a string that is refused is
named with its index.

The `i` output takes the init-time form, a `k` output the k form, which reads
the string at perf, on every pass its trigger is non-zero. The array form
takes an explicit trigger, and the call without one is the init-time form; it
runs again only when the source changed, and on a [real-time
path](csnrtlock.md) it allocates nothing while the source keeps its shape.

## Syntax

```csound
out:i = csnfromhex(source:S)
out:k = csnfromhex(source:S [, trig:k])
out:CsnArr = csnfromhex(source:CsnStr)
out:CsnArr = csnfromhex(source:CsnStr, trig:k)
```

## Arguments

* `source:S`: a string of hex digits, with or without `0x`.
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
; csnfromhex.csd
;
; A MIDI note-on written in hex, read back as its three bytes.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    top:i          = csnfromhex("0x7F")
    prints("0x7F is %d\n", top)
    words:S[]      = fillarray("90", "3C", "64")
    message:CsnStr = csnfromarray(words)
    bytes:CsnArr   = csnfromhex(message)
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
0x7F is 127
CsnArr(shape=(3,), dtype=float64)
[144 60 100]
```

## See also

* [csntohex](csntohex.md)
* [csnfrombinary](csnfrombinary.md)
* [csnstrtonum](csnstrtonum.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
