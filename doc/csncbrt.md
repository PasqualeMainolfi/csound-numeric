# csncbrt

## Abstract

Cube root of every element.

## Description

`csncbrt` returns the cube root of every element. Unlike the square root it is
defined over the whole real line, so a negative element gives a negative root
rather than a NaN.

Over a complex array it is computed as `z^(1/3)`, the principal branch.

Real and complex arrays are both accepted: a complex source is carried through
the complex form of the function and the result stays complex.

## Syntax

```csound
handle:CsnArr = csncbrt(source:CsnArr)
handle:CsnArr = csncbrt(source:CsnArr, trig:k)
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
; csncbrt.csd
;
; The cube root is defined for negative numbers too, which is what separates it
; from csnsqrt over the reals.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(8, 27, -8, -27))

    roots:CsnArr  = csncbrt(data)
    roots_out:i[] = csntoarray(roots)
    prints("cbrt = %g %g %g %g\n", roots_out[0], roots_out[1], roots_out[2], roots_out[3])

    ; no NaN, unlike the square root
    nan_cbrt:i    = csncntnan(roots)
    nan_sqrt:i    = csncntnan(csnsqrt(data))
    prints("NaN from cbrt = %d, from sqrt = %d\n", nan_cbrt, nan_sqrt)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsqrt](csnsqrt.md)
* [csnpow](csnpow.md)
* [csnexp](csnexp.md)

## Credits

Pasquale Mainolfi, 2026
