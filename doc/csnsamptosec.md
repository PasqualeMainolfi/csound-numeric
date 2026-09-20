# csnsamptosec

## Abstract

Sample counts into seconds at a given sample rate.

## Description

`csnsamptosec` converts samples into seconds:

```
n / sr
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
frame boundaries or delay taps converts in one call. [csnsectosamp](csnsectosamp.md) is the inverse,
and the round trip is exact up to floating-point rounding.

The sample rate is init-time on every overload, the k-rate form included: it is
a property of the material, and fixing it at init is what lets the performance
pass run without validating it again. It must be a positive whole number.

Real only.

## Syntax

```csound
value:i = csnsamptosec(samples:i, sr:i)
handle:CsnArr = csnsamptosec(samples:CsnArr, sr:i)
value:k = csnsamptosec(samples:k, sr:i)
handle:CsnArr = csnsamptosec(samples:CsnArr, sr:i, trig:k)
```

## Arguments

* `samples:i / samples:k`: one value in samples, for the scalar form.
* `samples:CsnArr`: any shape, for the array form.
* `sr:i`: the sample rate to convert against, a positive whole number. Init-time on every overload.
* `trig:k` (optional, default `1`): k-rate trigger on the array form. A zero trigger republishes the previous result.

## Output

* `value:i / value:k`: the value in seconds, for the scalar form.
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
; csnsamptosec.csd
;
; Sample counts into seconds at a chosen rate: n / sr. The same conversion
; csnsamptomillis performs, on the unit Csound scores are written in.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    one:i           = csnsamptosec(44100, 44100)
    prints("44100 sm at 44100: %g s\n", one)

    other:i         = csnsamptosec(44100, 48000)
    prints("44100 sm at 48000: %g s\n", other)

    ; frame boundaries of an analysis, as score times
    frames:CsnArr   = csnfromarray(array(0, 512, 1024, 1536, 2048))
    seconds:CsnArr  = csnsamptosec(frames, 44100)
    csnprint seconds

    ; the hop between two of them
    hop:i           = csnsamptosec(512, 44100)
    prints("hop of 512      : %g s\n", hop)

    ; round trip
    back:CsnArr     = csnsectosamp(seconds, 44100)
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
* [csnmillistosamp](csnmillistosamp.md)
* [csnsectosamp](csnsectosamp.md)
* [csnround](csnround.md)

## Credits

Pasquale Mainolfi, 2026
