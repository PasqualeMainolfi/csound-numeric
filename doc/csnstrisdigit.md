# csnstrisdigit

## Abstract

Elementwise test for digits only, as `np.strings.isdigit`.

## Description

`csnstrisdigit` returns, for each string, `1` when every byte of it is an
ASCII digit (`0`-`9`) and `0` otherwise, in a real array with the shape of the
source: a mask for [csnwhere](csnwhere.md), [csnselect](csnselect.md) or
[csncompress](csncompress.md). An empty string gives `0`, as in Python.

Only ASCII counts, whatever the locale: a byte from 0x80 up is never a digit,
so a string with a UTF-8 character such as `"é"` gives `0`. This is what
`np.strings.isdigit` does on a bytes (`S`) array; on a `str` array NumPy
follows Unicode.

The k form takes an explicit trigger, and the call without one is the
init-time form. It runs again only when the source changed.

The output is a real array in the shape of the source, so the k form writes
its numbers into the elements the init pass laid out and allocates nothing
while the source keeps its shape: it may run on a [real-time
path](csnrtlock.md).

On one `S` it returns a number, 1 or 0: `csnstrisdigit("42")` gives 1. The `i`
output takes the init-time form, a `k` output the k form. An `S` has no rate,
so the k form takes a trigger, and computes at init too.

## Syntax

```csound
out:CsnArr = csnstrisdigit(source:CsnStr)
out:CsnArr = csnstrisdigit(source:CsnStr, trig:k)
out:i = csnstrisdigit(source:S)
out:k = csnstrisdigit(source:S [, trig:k])
```

## Arguments

* `source:CsnStr`: the strings to query; or `source:S`, one string.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger republishes the previous result.

## Output

* `out:CsnArr`: a 0/1 mask in the shape of `source`.
* `out:i`, `out:k`: 1 or 0, for an `S` source.

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
; csnstrisdigit.csd
;
; Which tokens hold digits only: the empty one and the accented one
; do not.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    words:S[]    = fillarray("abc", "123", "a1", " \t", "", "é")
    tokens:CsnStr = csnfromarray(words)
    mask:CsnArr  = csnstrisdigit(tokens)
    csnprint(mask)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
CsnArr(shape=(6,), dtype=float64)
[0 1 0 0 0 0]
```

## See also

* [csnstrisalpha](csnstrisalpha.md)
* [csnstrisalnum](csnstrisalnum.md)
* [csnstrisspace](csnstrisspace.md)
* [csnstrlen](csnstrlen.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
