# csnlog

## Abstract

Logarithm of an array in an arbitrary base.

## Description

`csnlog(x, base)` returns the logarithm of every element of `x` in the given
base. The base is the second argument, so `csnlog(data, 2)` is the binary
logarithm and `csnlog(data, 10)` the decimal one.

Both operands may be arrays, broadcast against each other NumPy-style, and both
scalar orders exist: `csnlog(value, bases)` computes the logarithm of one number
in every base of an array.

There is no natural-logarithm shortcut — pass `2.718281828459045` as the base, or
work the other way with [csnexp](csnexp.md).

Real and complex arrays are both accepted, and an operation mixing the two
promotes the result to complex.

## Syntax

```csound
handle:CsnArr = csnlog(x:CsnArr, base:CsnArr)
handle:CsnArr = csnlog(x:CsnArr, base:CsnArr, trig:k)
handle:CsnArr = csnlog(x:CsnArr, base:i)
handle:CsnArr = csnlog(x:CsnArr, base:k)
handle:CsnArr = csnlog(x:CsnArr, base:k, trig:k)
handle:CsnArr = csnlog(x:CsnArr, base:Complex)
handle:CsnArr = csnlog(x:CsnArr, base:Complex, trig:k)
handle:CsnArr = csnlog(value:i, base:CsnArr)
handle:CsnArr = csnlog(value:k, base:CsnArr)
handle:CsnArr = csnlog(value:k, base:CsnArr, trig:k)
handle:CsnArr = csnlog(value:Complex, base:CsnArr)
handle:CsnArr = csnlog(value:Complex, base:CsnArr, trig:k)
```

## Arguments

* `x:CsnArr`: the array whose logarithm is taken.
* `base:CsnArr / base:i / base:k / base:Complex`: the base.
* `value:i / value:k / value:Complex`: a scalar whose logarithm is taken in every base of `base:CsnArr`.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the result.

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
; csnlog.csd
;
; The base is the second argument. Base 2 over a frequency array gives pitch in
; octaves, which is the commonest use in an orchestra.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(8, 64, 1024))

    binary:CsnArr  = csnlog(data, 2)
    binary_out:i[] = csntoarray(binary)
    prints("log2  = %g %g %g\n", binary_out[0], binary_out[1], binary_out[2])

    dec:CsnArr     = csnlog(data, 10)
    dec_out:i[]    = csntoarray(dec)
    prints("log10 = %.4f %.4f %.4f\n", dec_out[0], dec_out[1], dec_out[2])

    ; frequencies to octaves above 55 Hz
    freq:CsnArr    = csnfromarray(array(55, 110, 440))
    rel:CsnArr     = csndiv(freq, 55)
    oct:CsnArr     = csnlog(rel, 2)
    oct_out:i[]    = csntoarray(oct)
    prints("octaves = %g %g %g\n", oct_out[0], oct_out[1], oct_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnexp](csnexp.md)
* [csnpow](csnpow.md)
* [csnlogspace](csnlogspace.md)

## Credits

Pasquale Mainolfi, 2026
