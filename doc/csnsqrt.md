# csnsqrt

## Abstract

Square root of every element.

## Description

`csnsqrt` returns the square root of every element.

Over a real array the root of a negative number is a NaN — a real square root has
no other answer — so guard the input with [csnclip](csnclip.md) or locate the
results with [csnargisnan](csnargisnan.md). Over a complex array the principal
root is returned instead, and negative reals come back on the imaginary axis.

Real and complex arrays are both accepted: a complex source is carried through
the complex form of the function and the result stays complex.

## Syntax

```csound
handle:CsnArr = csnsqrt(source:CsnArr)
handle:CsnArr = csnsqrt(source:CsnArr, trig:k)
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
; csnsqrt.csd
;
; A real square root of a negative number is a NaN; the complex form returns the
; principal root instead.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(4, 9, 16, 25))

    roots:CsnArr   = csnsqrt(data)
    roots_out:i[]  = csntoarray(roots)
    prints("sqrt = %g %g %g %g\n", roots_out[0], roots_out[1], roots_out[2], roots_out[3])

    ; negatives give NaN over the reals
    mixed:CsnArr   = csnfromarray(array(4, -1, 9, -16))
    bad:CsnArr     = csnsqrt(mixed)
    nan_count:i    = csncntnan(bad)
    prints("NaN in the real root = %d\n", nan_count)

    ; over the complex field there is a principal root
    cpx:CsnArr     = csntocomplex(mixed)
    good:CsnArr    = csnsqrt(cpx)
    cell:i[]       = fillarray(1)
    z:Complex      = csnget(good, cell)
    z_re:i         = real(z)
    z_im:i         = imag(z)
    prints("sqrt(-1) = %g%+gi\n", z_re, z_im)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csncbrt](csncbrt.md)
* [csnpow](csnpow.md)
* [csnargisnan](csnargisnan.md)
* [csnabs](csnabs.md)

## Credits

Pasquale Mainolfi, 2026
