# csnatan

## Abstract

Arctangent of every element.

## Description

`csnatan` returns the arctangent of every element, in radians, in the range
`(-pi/2, pi/2)`.

Unlike the arcsine and the arccosine it accepts any real value, so no clipping is
needed. It is a single-argument function: for the angle of a point given by two
components, use [csnangle](csnangle.md) on a complex array.

Real and complex arrays are both accepted: a complex source is carried through
the complex form of the function and the result stays complex.

## Syntax

```csound
handle:CsnArr = csnatan(source:CsnArr)
handle:CsnArr = csnatan(source:CsnArr, trig:k)
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
; csnatan.csd
;
; The result is in radians. csnradtodeg converts it back for reading.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(-1, -0.5, 0, 0.5, 1))
    ang:CsnArr    = csnatan(data)
    ang_out:i[]   = csntoarray(ang)
    prints("atan = %.4f %.4f %.4f %.4f %.4f\n", ang_out[0], ang_out[1], ang_out[2], ang_out[3], ang_out[4])

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

* [csnasin](csnasin.md)
* [csnacos](csnacos.md)
* [csntan](csntan.md)
* [csnangle](csnangle.md)

## Credits

Pasquale Mainolfi, 2026
