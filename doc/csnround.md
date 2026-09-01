# csnround

## Abstract

Round every element to the nearest integer.

## Description

`csnround` replaces every element by the nearest integer. Halves go to the
**even** neighbour, as in NumPy: `0.5` and `-0.5` both become `0`, `1.5` becomes
`2` and `2.5` becomes `2`. C's own `round` would send them away from zero
instead, so the two do not agree on exact halves.

Real only: rounding has no meaning over the complex field, and a complex array is
refused rather than rounded lane by lane.

To round to a resolution other than 1, scale first and scale back:
`csndiv(csnround(csnmul(data, 100)), 100)` rounds to two decimals.

## Syntax

```csound
handle:CsnArr = csnround(source:CsnArr)
handle:CsnArr = csnround(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: the array to round.
* `trig:k`: k-rate trigger. The result is recomputed on a non-zero trigger; a zero trigger republishes the previous one.

## Output

* `handle:CsnArr`: handle of the rounded array.

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
; csnround.csd
;
; Nearest integer, halves to the even neighbour as in NumPy. Scale up and back
; down to round to a resolution other than 1.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(-1.2, 0.5, 1.5, 2.5, 1.7))

    near:CsnArr    = csnround(data)
    near_out:i[]   = csntoarray(near)
    prints("round = %g %g %g %g %g\n", near_out[0], near_out[1], near_out[2], near_out[3], near_out[4])

    ; quantise a set of frequency ratios to two decimals
    ratios:CsnArr  = csnfromarray(array(1.4983, 1.2599, 1.0595))
    scaled:CsnArr  = csnmul(ratios, 100)
    q:CsnArr       = csndiv(csnround(scaled), 100)
    q_out:i[]      = csntoarray(q)
    prints("two decimals = %g %g %g\n", q_out[0], q_out[1], q_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnfloor](csnfloor.md)
* [csnceil](csnceil.md)
* [csnmul](csnmul.md)

## Credits

Pasquale Mainolfi, 2026
