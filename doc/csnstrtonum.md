# csnstrtonum

## Abstract

Elementwise string to number, as `astype(float)`.

## Description

`csnstrtonum` reads each string as a number, in a real array with the shape
of the source. A string reads as Python's `float()` reads it: blanks around
it, an optional sign, then digits with an optional decimal point and exponent
(`"440"`, `" -2.5e3\n"`, `".5"`), or `inf` / `infinity` in any case. The
point is always `.`, whatever locale the host runs in.

A string that is not a number gives `NaN`: an empty one, a word, a number with
more text after it (`"12abc"`), a decimal comma (`"1,5"`), a hex number
(`"0x10"`) and `"nan"` itself. NumPy raises an error there instead; here one
bad entry does not stop the note, and [csnisnan](csnisnan.md) finds it.
Python also reads digit underscores (`"1_000"`); here they give `NaN`.

The k form takes an explicit trigger, and the call without one is the
init-time form. It runs again only when the source changed. The output is a
real array in the shape of the source, so the k form writes its numbers into
the elements the init pass laid out and allocates nothing while the source
keeps its shape: it may run on a [real-time path](csnrtlock.md).

## Syntax

```csound
out:CsnArr = csnstrtonum(source:CsnStr)
out:CsnArr = csnstrtonum(source:CsnStr, trig:k)
```

## Arguments

* `source:CsnStr`: the strings to read.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger republishes the previous result.

## Output

* `out:CsnArr`: the numbers, `NaN` where a string is not one, in the shape of `source`.

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
; csnstrtonum.csd
;
; Frequencies typed as text, one of them by name: the name and the empty
; field read as NaN.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    fields:S[]   = fillarray("440", " 220.5 ", "1e3", "A4", "")
    text:CsnStr  = csnfromarray(fields)
    freqs:CsnArr = csnstrtonum(text)
    csnprint(freqs)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
CsnArr(shape=(5,), dtype=float64)
[440 220.5 1000 nan nan]
```

## See also

* [csnnumtostr](csnnumtostr.md)
* [csnisnan](csnisnan.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
