# csnasin

## Abstract

Arcsine of every element.

## Description

`csnasin` returns the arcsine of every element, in radians, in the range
`[-pi/2, pi/2]`.

Over the reals the argument must lie in `[-1, 1]`; anything outside gives a NaN, so
[csnclip](csnclip.md) is the usual guard. A complex array has no such restriction.

Real and complex arrays are both accepted: a complex source is carried through
the complex form of the function and the result stays complex.

## Syntax

```csound
handle:CsnArr = csnasin(source:CsnArr)
handle:CsnArr = csnasin(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: the array to read.
* `trig:k`: k-rate trigger. The result is recomputed on a non-zero trigger; a zero trigger republishes the previous one.

## Output

* `handle:CsnArr`: handle of the result, with the shape and element type of the source.

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
; csnasin.csd
;
; The result is in radians. csnradtodeg converts it back for reading.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(-1, -0.5, 0, 0.5, 1))
    safe:CsnArr   = csnclip(data, -1, 1)
    ang:CsnArr    = csnasin(safe)
    ang_out:i[]   = csntoarray(ang)
    prints("asin = %.4f %.4f %.4f %.4f %.4f\n", ang_out[0], ang_out[1], ang_out[2], ang_out[3], ang_out[4])

    ; radians out, degrees for reading
    deg:CsnArr    = csnradtodeg(ang)
    deg_out:i[]   = csntoarray(deg)
    prints("degrees = %.2f %.2f %.2f %.2f %.2f\n", deg_out[0], deg_out[1], deg_out[2], deg_out[3], deg_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnacos](csnacos.md)
* [csnatan](csnatan.md)
* [csnsin](csnsin.md)
* [csnclip](csnclip.md)

## Credits

Pasquale Mainolfi, 2026
