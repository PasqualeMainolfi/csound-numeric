# csnsamptomillis

## Abstract

Sample counts into milliseconds at a given sample rate.

## Description

`csnsamptomillis` converts samples into milliseconds:

```
1000 * n / sr
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
frame boundaries or delay taps converts in one call. [csnmillistosamp](csnmillistosamp.md) is the inverse,
and the round trip is exact up to floating-point rounding.

The sample rate is init-time on every overload, the k-rate form included: it is
a property of the material, and fixing it at init is what lets the performance
pass run without validating it again. It must be a positive whole number.

Real only.

## Syntax

```csound
value:i = csnsamptomillis(samples:i, sr:i)
handle:CsnArr = csnsamptomillis(samples:CsnArr, sr:i)
value:k = csnsamptomillis(samples:k, sr:i)
handle:CsnArr = csnsamptomillis(samples:CsnArr, sr:i, trig:k)
```

## Arguments

* `samples:i / samples:k`: one value in samples, for the scalar form.
* `samples:CsnArr`: any shape, for the array form.
* `sr:i`: the sample rate to convert against, a positive whole number. Init-time on every overload.
* `trig:k` (optional, default `1`): k-rate trigger on the array form. A zero trigger republishes the previous result.

## Output

* `value:i / value:k`: the value in milliseconds, for the scalar form.
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
; csnsamptomillis.csd
;
; Sample counts into milliseconds at a chosen rate: 1000 n / sr. The sample rate
; is an argument rather than the orchestra's, so a file recorded at one rate can
; be measured while the orchestra runs at another. Scalar and array forms.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; one value
    one:i           = csnsamptomillis(4410, 44100)
    prints("4410 sm at 44100: %g ms\n", one)

    ; the same count read at another rate
    other:i         = csnsamptomillis(4410, 48000)
    prints("4410 sm at 48000: %g ms\n", other)

    ; a whole set of onsets at once
    onsets:CsnArr   = csnfromarray(array(0, 4410, 11025, 22050, 44100))
    millis:CsnArr   = csnsamptomillis(onsets, 44100)
    csnprint millis

    ; and back, which is what csnmillistosamp is for
    back:CsnArr     = csnmillistosamp(millis, 44100)
    csnprint back

    ; a delay line length in milliseconds, from a length in samples
    tap:i           = csnsamptomillis(2205, 44100)
    prints("2205 samples    : %g ms\n", tap)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsamptosec](csnsamptosec.md)
* [csnmillistosamp](csnmillistosamp.md)
* [csnsectosamp](csnsectosamp.md)
* [csnround](csnround.md)

## Credits

Pasquale Mainolfi, 2026
