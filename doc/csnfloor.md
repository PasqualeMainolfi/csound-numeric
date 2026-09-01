# csnfloor

## Abstract

Round every element down to the nearest integer.

## Description

`csnfloor` replaces every element by the largest integer not greater than it, so
`-1.2` becomes `-2` rather than `-1`. That is the rounding
[csndivmod](csndivmod.md) uses for its quotient, and the one that makes a
floor-then-modulo pair usable as a wrapped index.

Real only: rounding has no meaning over the complex field, and a complex array is
refused rather than rounded lane by lane.

## Syntax

```csound
handle:CsnArr = csnfloor(source:CsnArr)
handle:CsnArr = csnfloor(source:CsnArr, trig:k)
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
; csnfloor.csd
;
; Floor goes down, always: -1.2 becomes -2. That is what separates it from
; csnround, which goes to the nearest.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr  = csnfromarray(array(-1.2, -0.5, 0.5, 1.7))

    down:CsnArr  = csnfloor(data)
    down_out:i[] = csntoarray(down)
    prints("floor = %g %g %g %g\n", down_out[0], down_out[1], down_out[2], down_out[3])

    ; the three roundings side by side
    up:CsnArr    = csnceil(data)
    near:CsnArr  = csnround(data)
    up_out:i[]   = csntoarray(up)
    near_out:i[] = csntoarray(near)
    prints("ceil  = %g %g %g %g\n", up_out[0], up_out[1], up_out[2], up_out[3])
    prints("round = %g %g %g %g   (halves go to the even neighbour)\n", near_out[0], near_out[1], near_out[2], near_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnceil](csnceil.md)
* [csnround](csnround.md)
* [csndivmod](csndivmod.md)

## Credits

Pasquale Mainolfi, 2026
