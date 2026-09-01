# csntan

## Abstract

Tangent of every element.

## Description

`csntan` returns the tangent of every element, the argument in **radians**. The
tangent has no value at odd multiples of pi/2, where the result runs off to a very
large number rather than an exact infinity.

Real and complex arrays are both accepted: a complex source is carried through
the complex form of the function and the result stays complex.

## Syntax

```csound
handle:CsnArr = csntan(source:CsnArr)
handle:CsnArr = csntan(source:CsnArr, trig:k)
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
; csntan.csd
;
; The argument is in radians. csndegtorad converts, and csnlinspace is the
; usual way to lay out a phase axis.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    phase:CsnArr  = csnlinspace(0, 6.283185307179586, 5)

    out_a:CsnArr  = csntan(phase)
    out_arr:i[]   = csntoarray(out_a)
    prints("tangent = %.4f %.4f %.4f %.4f %.4f\n", out_arr[0], out_arr[1], out_arr[2], out_arr[3], out_arr[4])

    ; the same thing from degrees
    deg:CsnArr    = csnfromarray(array(0, 90, 180, 270, 360))
    rad:CsnArr    = csndegtorad(deg)
    from_deg:CsnArr = csntan(rad)
    deg_out:i[]   = csntoarray(from_deg)
    prints("again = %.4f %.4f %.4f %.4f %.4f\n", deg_out[0], deg_out[1], deg_out[2], deg_out[3], deg_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsin](csnsin.md)
* [csncos](csncos.md)
* [csnatan](csnatan.md)
* [csndegtorad](csndegtorad.md)

## Credits

Pasquale Mainolfi, 2026
