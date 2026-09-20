# csnfschrd

## Abstract

The Schroeder frequency of a room, the crossover between its modal and diffuse regions.

## Description

`csnfschrd` answers Schroeder's frequency:

```
f = 2000 * sqrt(T60 / V)
```

with `T60` in seconds and `V` in cubic metres. Below it a room behaves modally:
the resonances of [csnfpqr](csnfpqr.md) are far enough apart to be heard one by
one, and what you hear depends strongly on where you stand. Above it they crowd
together and overlap into a statistical, diffuse field, which is the regime
Sabine and Eyring assume in the first place.

The number decides which tool applies. Reverberation design, ray tracing and
[csnt60sab](csnt60sab.md) are meaningful above it; below it only modal analysis,
placement and low-frequency treatment are. In a small room the crossover lands
in the low hundreds of hertz, so most of what matters musically is modal; in a
concert hall it falls near 20 Hz and essentially everything audible is diffuse.

The constant 2000 is the usual SI form; some texts quote 2000, others 1000 or
the metric-adjusted 2000/sqrt(2), so a number from elsewhere may differ by a
constant factor.

Four overloads cover the four ways the two operands can arrive. Both scalar
gives a scalar. One scalar and one array gives a one-dimensional array of that
array's length. Both arrays gives a two-dimensional result of
`volumes x t60s`, every volume paired with every reverberation time — the row
index follows the volume, the column index the time.

Real only. A volume or a reverberation time of zero, or a negative one, answers
`0` rather than an infinity or a NaN.

The k-rate overloads follow the same four shapes. The scalar/scalar one carries
no trigger — it is a division and a square root, recomputed every control
period — while the three that publish a handle take a trailing trigger, and a
zero trigger republishes the previous result.

## Syntax

```csound
value:i = csnfschrd(volume:i, t60:i)
handle:CsnArr = csnfschrd(volume:i, t60s:CsnArr)
handle:CsnArr = csnfschrd(volumes:CsnArr, t60:i)
handle:CsnArr = csnfschrd(volumes:CsnArr, t60s:CsnArr)
value:k = csnfschrd(volume:k, t60:k)
handle:CsnArr = csnfschrd(volume:k, t60s:CsnArr, trig:k)
handle:CsnArr = csnfschrd(volumes:CsnArr, t60:k, trig:k)
handle:CsnArr = csnfschrd(volumes:CsnArr, t60s:CsnArr, trig:k)
```

## Arguments

* `volume:i / volume:k`, `volumes:CsnArr`: the room volume in cubic metres, one value or a one-dimensional array of them.
* `t60:i / t60:k`, `t60s:CsnArr`: the reverberation time in seconds, one value or a one-dimensional array of them.
* `trig:k` (optional, default `1`): k-rate trigger on the overloads that publish a handle. A zero trigger republishes the previous result.

## Output

* `value:i / value:k`: the Schroeder frequency in hertz, when both operands are scalars.
* `handle:CsnArr`: one value per element when one operand is an array; a `volumes x t60s` matrix when both are.

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
; csnfschrd.csd
;
; The Schroeder frequency, 2000 sqrt(T60 / V): the crossover between the modal
; region of a room, where individual resonances are separate and audible, and
; the diffuse region above it, where they overlap into a statistical field.
; Reverb design lives above it; room correction lives below.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; a small live room: the modal region reaches high
    small:i          = csnfschrd(50, 1.2)
    prints("50 m3, 1.2 s    : %.2f Hz\n", small)

    ; a concert hall: almost everything audible is already diffuse
    hall:i           = csnfschrd(12000, 2.0)
    prints("12000 m3, 2.0 s : %.2f Hz\n", hall)

    ; one room, several possible treatments
    targets:CsnArr   = csnfromarray(array(0.4, 0.8, 1.6))
    curve:CsnArr     = csnfschrd(200, targets)
    csnprint curve

    ; several rooms, one reverberation time
    volumes:CsnArr   = csnfromarray(array(50, 200, 800))
    by_room:CsnArr   = csnfschrd(volumes, 1.2)
    csnprint by_room

    ; every room against every target
    grid:CsnArr      = csnfschrd(volumes, targets)
    csnprint grid
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnfpqr](csnfpqr.md)
* [csnt60sab](csnt60sab.md)
* [csnt60eyr](csnt60eyr.md)
* [csnrt60absp](csnrt60absp.md)

## Credits

Pasquale Mainolfi, 2026
