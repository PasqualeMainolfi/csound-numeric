# csnsign

## Abstract

Return -1, 0 or 1 by the sign of each element.

## Description

`csnsign` replaces every element by `-1` if it is negative, `1` if it is
positive, and `0` if it is zero. NaN is carried through, following NumPy: the
sign of a NaN is a NaN, and the sign of zero is zero rather than 1.

For a complex array the result is the unit-magnitude complex number in the same
direction, `z / |z|`, with zero mapping to zero.

Multiplied back into [csnabs](csnabs.md) it reconstructs the original, which is
the usual way to process a magnitude and put the sign back afterwards.

## Syntax

```csound
handle:CsnArr = csnsign(source:CsnArr)
handle:CsnArr = csnsign(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: the array to read.
* `trig:k`: k-rate trigger. The result is recomputed on a non-zero trigger; a zero trigger republishes the previous one.

## Output

* `handle:CsnArr`: handle of the signs.

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
; csnsign.csd
;
; Split a signal into sign and magnitude, shape the magnitude, put the sign
; back: csnsign and csnabs are the two halves of that idiom.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(-3, 0, 2, -0.5))

    sgn:CsnArr     = csnsign(data)
    sgn_out:i[]    = csntoarray(sgn)
    prints("sign = %g %g %g %g\n", sgn_out[0], sgn_out[1], sgn_out[2], sgn_out[3])

    ; soft-clip the magnitude, then restore the sign
    mag:CsnArr     = csnabs(data)
    shaped:CsnArr  = csntanh(mag)
    signed:CsnArr  = csnmul(shaped, sgn)
    signed_out:i[] = csntoarray(signed)
    prints("shaped = %.4f %.4f %.4f %.4f\n", signed_out[0], signed_out[1], signed_out[2], signed_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnabs](csnabs.md)
* [csnclip](csnclip.md)
* [csngt](csngt.md)

## Credits

Pasquale Mainolfi, 2026
