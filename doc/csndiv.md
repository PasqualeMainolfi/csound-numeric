# csndiv

## Abstract

Elementwise division of two arrays, or of an array and a scalar.

## Description

`csndiv` divides its first operand by its second, elementwise. Two arrays are
broadcast against each other NumPy-style — aligned from the last axis, each pair
of extents must either match or be 1.

Division is not commutative, so both scalar orders exist:
`csndiv(array, value)` divides every element by the scalar, and
`csndiv(value, array)` divides the scalar by every element, which is how to build
a reciprocal: `csndiv(1, data)`.

**Division by zero is refused**, at init or at the k-pass where it happens,
rather than producing an infinity or a NaN. Guard a divisor that may reach zero
with [csnclip](csnclip.md) or [csncnteq](csncnteq.md).

Real and complex arrays are both accepted, and an operation mixing the two
promotes the result to complex.

## Syntax

```csound
handle:CsnArr = csndiv(a:CsnArr, b:CsnArr)
handle:CsnArr = csndiv(a:CsnArr, b:CsnArr, trig:k)
handle:CsnArr = csndiv(a:CsnArr, value:i)
handle:CsnArr = csndiv(a:CsnArr, value:k)
handle:CsnArr = csndiv(a:CsnArr, value:k, trig:k)
handle:CsnArr = csndiv(a:CsnArr, value:Complex)
handle:CsnArr = csndiv(a:CsnArr, value:Complex, trig:k)
handle:CsnArr = csndiv(value:i, b:CsnArr)
handle:CsnArr = csndiv(value:k, b:CsnArr)
handle:CsnArr = csndiv(value:k, b:CsnArr, trig:k)
handle:CsnArr = csndiv(value:Complex, b:CsnArr)
handle:CsnArr = csndiv(value:Complex, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`: the dividend.
* `b:CsnArr`: the divisor; broadcast against `a`. No element may be zero.
* `value:i / value:k / value:Complex`: a scalar, on either side of the operation.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the quotient.

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
; csndiv.csd
;
; Both scalar orders exist; the scalar on the left is how a reciprocal is
; written. A zero divisor is an error, not an infinity.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr      = csnfromarray(array(10, 20, 30))
    b:CsnArr      = csnfromarray(array(2, 4, 5))

    quot:CsnArr   = csndiv(a, b)
    quot_out:i[]  = csntoarray(quot)
    prints("array / array  = %g %g %g\n", quot_out[0], quot_out[1], quot_out[2])

    half:CsnArr   = csndiv(a, 10)
    half_out:i[]  = csntoarray(half)
    prints("array / scalar = %g %g %g\n", half_out[0], half_out[1], half_out[2])

    recip:CsnArr  = csndiv(1, b)
    recip_out:i[] = csntoarray(recip)
    prints("reciprocal     = %g %g %g\n", recip_out[0], recip_out[1], recip_out[2])

    ; keep a divisor away from zero before dividing by it
    raw:CsnArr    = csnfromarray(array(0, 0.5, 2))
    safe:CsnArr   = csnclip(raw, 0.001, 1000)
    ratio:CsnArr  = csndiv(a, safe)
    ratio_out:i[] = csntoarray(ratio)
    prints("guarded        = %g %g %g\n", ratio_out[0], ratio_out[1], ratio_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnmul](csnmul.md)
* [csndivmod](csndivmod.md)
* [csnclip](csnclip.md)
* [csnpow](csnpow.md)

## Credits

Pasquale Mainolfi, 2026
