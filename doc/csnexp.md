# csnexp

## Abstract

Exponential of every element.

## Description

`csnexp` returns `e^x` for every element. It is the inverse of the natural
logarithm, so `csnexp` undoes `csnlog(data, 2.718281828459045)` and the other way
round.

For an exponential over another base, use [csnpow](csnpow.md) with the base as its
first argument: `csnpow(2, data)`.

Real and complex arrays are both accepted: a complex source is carried through
the complex form of the function and the result stays complex.

## Syntax

```csound
handle:CsnArr = csnexp(source:CsnArr)
handle:CsnArr = csnexp(source:CsnArr, trig:k)
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
; csnexp.csd
;
; csnexp is e^x elementwise. An exponential over another base is csnpow with
; the base on the left.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(0, 1, 2, 3))

    exp_a:CsnArr  = csnexp(data)
    exp_out:i[]   = csntoarray(exp_a)
    prints("exp   = %.4f %.4f %.4f %.4f\n", exp_out[0], exp_out[1], exp_out[2], exp_out[3])

    ; and back again through the natural logarithm
    back:CsnArr   = csnlog(exp_a, 2.718281828459045)
    back_out:i[]  = csntoarray(back)
    prints("round trip = %g %g %g %g\n", back_out[0], back_out[1], back_out[2], back_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnlog](csnlog.md)
* [csnpow](csnpow.md)
* [csnsqrt](csnsqrt.md)

## Credits

Pasquale Mainolfi, 2026
