# csnabs

## Abstract

Absolute value of every element, or magnitude for a complex array.

## Description

`csnabs` returns the absolute value of every element of a real array, and the
magnitude — `sqrt(re² + im²)` — of every element of a complex one. A complex
input therefore yields a **real** result, which is the one case in the
elementwise family where the element type changes.

Each complex element is read as one real/imaginary pair. Thus an input such as
`[3+0i, -4+0i, 0+0i, 5+0i]` produces `[3, 4, 0, 5]`, with the same logical
element count as the source.

For a real array it is the usual rectifier: the sign is dropped and NaN is
carried through.

## Syntax

```csound
handle:CsnArr = csnabs(source:CsnArr)
handle:CsnArr = csnabs(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: the array to read.
* `trig:k`: k-rate trigger. The result is recomputed on a non-zero trigger; a zero trigger republishes the previous one.

## Output

* `handle:CsnArr`: absolute values for a real source, magnitudes — as a real array — for a complex one.

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
; csnabs.csd
;
; csnabs drops the sign of a real array. Its usual companion is csnsum, which
; turns the pair into a sum of magnitudes.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr = csnfromarray(array(-3, 1, -4, 1, -5))

    mag:CsnArr  = csnabs(data)
    mag_out:i[] = csntoarray(mag)
    prints("abs = %g %g %g %g %g\n", mag_out[0], mag_out[1], mag_out[2], mag_out[3], mag_out[4])

    ; sum of magnitudes, the L1 norm read the long way
    total:i     = csnsum(mag)
    l1:i        = csnnorm(data, 1)
    prints("sum of abs = %g, csnnorm order 1 = %g\n", total, l1)

    ; Complex input still produces one real magnitude per complex element.
    complex_data:CsnArr = csntocomplex(csnfromarray(array(3, -4, 0, 5)))
    complex_mag:CsnArr = csnabs(complex_data)
    complex_out:i[] = csntoarray(complex_mag)
    prints("complex magnitudes = %g %g %g %g\n", complex_out[0], complex_out[1], complex_out[2], complex_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsign](csnsign.md)
* [csnhypot](csnhypot.md)
* [csnnorm](csnnorm.md)
* [csnangle](csnangle.md)

## Credits

Pasquale Mainolfi, 2026
