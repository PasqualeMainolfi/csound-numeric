# csnmillistosamp

## Abstract

Milliseconds into sample counts at a given sample rate.

## Description

`csnmillistosamp` converts milliseconds into samples:

```
ms * sr / 1000
```

The sample rate is an argument, not the orchestra's `sr`. That is the point of
the opcode: analysis data, file offsets and score material often carry a rate of
their own, and converting them against the rate they were made at is not the
same as converting them against the rate Csound happens to be running. Pass
`sr` explicitly when you do want the orchestra's.

Nothing is rounded. A duration that does not land on a sample boundary keeps its
fraction, and turning it into an integer is left to [csnround](csnround.md),
[csnfloor](csnfloor.md) or [csnceil](csnceil.md) — an interpolating reader may
well want the fraction.

The scalar form takes one value and answers one; the array form takes a handle
of any shape and answers a handle of the same shape, so a whole list of onsets,
frame boundaries or delay taps converts in one call. [csnsamptomillis](csnsamptomillis.md) is the inverse,
and the round trip is exact up to floating-point rounding.

The sample rate is init-time on every overload, the k-rate form included: it is
a property of the material, and fixing it at init is what lets the performance
pass run without validating it again. It must be a positive whole number.

Real only.

## Syntax

```csound
value:i = csnmillistosamp(milliseconds:i, sr:i)
handle:CsnArr = csnmillistosamp(milliseconds:CsnArr, sr:i)
value:k = csnmillistosamp(milliseconds:k, sr:i)
handle:CsnArr = csnmillistosamp(milliseconds:CsnArr, sr:i, trig:k)
```

## Arguments

* `milliseconds:i / milliseconds:k`: one value in milliseconds, for the scalar form.
* `milliseconds:CsnArr`: any shape, for the array form.
* `sr:i`: the sample rate to convert against, a positive whole number. Init-time on every overload.
* `trig:k` (optional, default `1`): k-rate trigger on the array form. A zero trigger republishes the previous result.

## Output

* `value:i / value:k`: the value in samples, for the scalar form.
* `handle:CsnArr`: the converted array, shaped like the source.

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
; csnmillistosamp.csd
;
; Milliseconds into sample counts at a chosen rate: ms sr / 1000. The result is
; not rounded: a duration that does not fall on a sample boundary keeps its
; fraction, and rounding it is left to whoever needs an integer.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    one:i           = csnmillistosamp(100, 44100)
    prints("100 ms at 44100 : %g samples\n", one)

    other:i         = csnmillistosamp(100, 48000)
    prints("100 ms at 48000 : %g samples\n", other)

    ; a set of delay taps in milliseconds
    taps:CsnArr     = csnfromarray(array(7, 13, 29, 53))
    in_samples:CsnArr = csnmillistosamp(taps, 44100)
    csnprint in_samples

    ; nothing is rounded: 7 ms is not a whole number of samples at 44100
    rounded:CsnArr  = csnround(in_samples)
    csnprint rounded

    ; and back
    back:CsnArr     = csnsamptomillis(in_samples, 44100)
    csnprint back
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsamptomillis](csnsamptomillis.md)
* [csnsamptosec](csnsamptosec.md)
* [csnsectosamp](csnsectosamp.md)
* [csnround](csnround.md)

## Credits

Pasquale Mainolfi, 2026
