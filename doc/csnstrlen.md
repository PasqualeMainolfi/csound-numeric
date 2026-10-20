# csnstrlen

## Abstract

Elementwise string length in bytes, as `np.strings.str_len`.

## Description

`csnstrlen` returns, for each string, the number of bytes it holds, in a real
array with the shape of the source. It counts bytes, as NumPy does on a bytes
(`S`) array, so a multibyte UTF-8 character counts once for each of its bytes:
`"Détaché"` is 9 bytes long. An empty string gives `0`.

The k form takes an explicit trigger, and the call without one is the
init-time form. It runs again only when the source changed.

The output is a real array in the shape of the source, so the k form writes
its numbers into the elements the init pass laid out and allocates nothing
while the source keeps its shape: it may run on a [real-time
path](csnrtlock.md).

## Syntax

```csound
out:CsnArr = csnstrlen(source:CsnStr)
out:CsnArr = csnstrlen(source:CsnStr, trig:k)
```

## Arguments

* `source:CsnStr`: the strings to query.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger republishes the previous result.

## Output

* `out:CsnArr`: the lengths in bytes, in the shape of `source`.

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
; csnstrlen.csd
;
; The length in bytes of each note name: the accented one counts its
; UTF-8 bytes.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    names:S[]    = fillarray("c4", "Eb4", "g#4", "Détaché")
    notes:CsnStr = csnfromarray(names)
    lens:CsnArr  = csnstrlen(notes)
    csnprint(lens)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
CsnArr(shape=(4,), dtype=float64)
[2 3 3 9]
```

## See also

* [csnstrfind](csnstrfind.md)
* [csnstrsubstr](csnstrsubstr.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
