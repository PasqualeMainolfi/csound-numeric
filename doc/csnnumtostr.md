# csnnumtostr

## Abstract

Elementwise number to string, as `astype(str)`.

## Description

`csnnumtostr` writes each number of a real array as a string, in a
`:CsnStr;` with the shape of the source. A number is written as NumPy's
`astype(str)` writes it, which is Python's `repr`: the fewest digits that read
back to the same number (`0.1`, not `0.10000000000000001`), a whole number
with `.0` (`"440.0"`), an exponent below `1e-4` and from `1e16` on
(`"1e-05"`, `"2e+16"`), and `nan`, `inf`, `-inf`. The point is always `.`,
whatever locale the host runs in, and [csnstrtonum](csnstrtonum.md) reads
every string back to the number it came from.

A complex array is refused: write its parts through [csnreal](csnreal.md) and
[csnimag](csnimag.md).

The k form takes an explicit trigger, and the call without one is the
init-time form. It runs again only when the source changed. No number takes
more than 24 bytes, so on a [real-time path](csnrtlock.md) the k init gives
every slot room for that and the k form writes each new string into its slot,
allocating nothing while the source keeps its shape.

## Syntax

```csound
out:CsnStr = csnnumtostr(source:CsnArr)
out:CsnStr = csnnumtostr(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: the real numbers to write.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger republishes the previous result.

## Output

* `out:CsnStr`: the strings, in the shape of `source`.

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
; csnnumtostr.csd
;
; Frequencies turned into labels: the shortest digits that give the number
; back, then a unit.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    hz:i[]        = fillarray(440, 261.6255653005986, 0.1, 1e-5, 2e16)
    freqs:CsnArr  = csnfromarray(hz)
    text:CsnStr   = csnnumtostr(freqs)
    csnprint(text)
    labels:CsnStr = text + " Hz"
    csnprint(labels)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
CsnStr(shape=(5,), dtype=str)
['440.0' '261.6255653005986' '0.1' '1e-05' '2e+16']
CsnStr(shape=(5,), dtype=str)
['440.0 Hz' '261.6255653005986 Hz' '0.1 Hz' '1e-05 Hz' '2e+16 Hz']
```

## See also

* [csnstrtonum](csnstrtonum.md)
* [csnstradd](csnstradd.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
