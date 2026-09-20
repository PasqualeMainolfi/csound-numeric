# csnsectosamp

## Abstract

Seconds into sample counts at a given sample rate.

## Description

`csnsectosamp` converts seconds into samples:

```
s * sr
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
frame boundaries or delay taps converts in one call. [csnsamptosec](csnsamptosec.md) is the inverse,
and the round trip is exact up to floating-point rounding.

The sample rate is init-time on every overload, the k-rate form included: it is
a property of the material, and fixing it at init is what lets the performance
pass run without validating it again. It must be a positive whole number.

Real only.

## Syntax

```csound
value:i = csnsectosamp(seconds:i, sr:i)
handle:CsnArr = csnsectosamp(seconds:CsnArr, sr:i)
value:k = csnsectosamp(seconds:k, sr:i)
handle:CsnArr = csnsectosamp(seconds:CsnArr, sr:i, trig:k)
```

## Arguments

* `seconds:i / seconds:k`: one value in seconds, for the scalar form.
* `seconds:CsnArr`: any shape, for the array form.
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
; csnsectosamp.csd
;
; Seconds into sample counts at a chosen rate: s sr. The companion of
; csnsamptosec, and the conversion that turns score times into buffer offsets.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    one:i           = csnsectosamp(1, 44100)
    prints("1 s at 44100    : %g samples\n", one)

    other:i         = csnsectosamp(1, 48000)
    prints("1 s at 48000    : %g samples\n", other)

    ; score times into offsets in a buffer
    times:CsnArr    = csnfromarray(array(0, 0.25, 0.5, 0.75, 1.0))
    offsets:CsnArr  = csnsectosamp(times, 44100)
    csnprint offsets

    ; a buffer long enough to hold the last of them
    total:i         = csnsectosamp(1.0, 44100)
    prints("buffer needed   : %g samples\n", total)

    ; and back
    back:CsnArr     = csnsamptosec(offsets, 44100)
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
* [csnmillistosamp](csnmillistosamp.md)
* [csnround](csnround.md)

## Credits

Pasquale Mainolfi, 2026
