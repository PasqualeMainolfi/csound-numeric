# csnt60sab

## Abstract

Sabine reverberation time from a room volume and its absorbing surfaces.

## Description

`csnt60sab` answers the Sabine estimate of T60, the time a sound takes to decay
by 60 dB:

```
T60 = 0.161 * V / A        A = sum over the surfaces of S_i * alpha_i
```

`V` is the volume in cubic metres, `S_i` the area of each surface in square
metres and `alpha_i` its absorption coefficient between 0 and 1. The absorption
`A` is in sabins; note that it is the *product* of each area and its
coefficient, summed, not the sum of the two.

Sabine assumes a diffuse field and absorption spread evenly over the boundary.
It is the estimate every handbook opens with, and it is accurate while the mean
coefficient stays low. As the room gets more absorbent it keeps reporting a tail
that is not there — in the limit of a fully absorbing room it still answers a
positive time, which is why [csnt60eyr](csnt60eyr.md) exists.

Two shapes of input. The scalar form takes one volume with a one-dimensional
list of surfaces and the matching list of coefficients, and answers one time.
The array form takes a one-dimensional array of volumes with a matrix of
surfaces and a matrix of coefficients, one row per volume, and answers one time
per room. The two matrices must agree with each other and their row count must
equal the number of volumes.

Real only, and the coefficients are not clamped: a coefficient outside 0..1 is
your own business. An absorption of zero yields `0` rather than an infinity,
since a room that absorbs nothing has no meaningful decay time to report.

At k-rate the scalar form takes the volume at k-rate too, and both forms hold
their result while the sources and the volume are unchanged.

## Syntax

```csound
value:i = csnt60sab(volume:i, surfaces:CsnArr, alphas:CsnArr)
handle:CsnArr = csnt60sab(volumes:CsnArr, surfaces:CsnArr, alphas:CsnArr)
value:k = csnt60sab(volume:k, surfaces:CsnArr, alphas:CsnArr, trig:k)
handle:CsnArr = csnt60sab(volumes:CsnArr, surfaces:CsnArr, alphas:CsnArr, trig:k)
```

## Arguments

* `volume:i / volume:k`: the room volume in cubic metres.
* `volumes:CsnArr`: one volume per room, one-dimensional.
* `surfaces:CsnArr`: the surface areas in square metres — one-dimensional in the scalar form, one row per room in the array form.
* `alphas:CsnArr`: the absorption coefficient of each surface, shaped like `surfaces`.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result instead of recomputing.

## Output

* `value:i / value:k`: the reverberation time in seconds, for the scalar form.
* `handle:CsnArr`: one reverberation time per volume, one-dimensional, for the array form.

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
; csnt60sab.csd
;
; Sabine's reverberation time. The absorption of a room is the sum of every
; surface times its own coefficient, and T60 is 0.161 V / A. The scalar form
; takes one room; the array form takes one volume per row and the matching row
; of surfaces and coefficients, so a whole set of rooms is answered at once.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; a 200 m3 room: 60 m2 of plaster, 40 m2 of carpet, 30 m2 of curtain
    surfaces:CsnArr = csnfromarray(array(60, 40, 30))
    alphas:CsnArr   = csnfromarray(array(0.1, 0.25, 0.6))

    ; absorption = 60*0.1 + 40*0.25 + 30*0.6 = 34 sabins
    t60:i           = csnt60sab(200, surfaces, alphas)
    prints("T60 (200 m3)    : %.4f s\n", t60)

    ; doubling the volume doubles the time: nothing absorbs any more than before
    t60_big:i       = csnt60sab(400, surfaces, alphas)
    prints("T60 (400 m3)    : %.4f s\n", t60_big)

    ; the same two rooms in one call: one volume per row, one row of surfaces
    ; and of coefficients each
    shape:i[]       = fillarray(2, 3)
    volumes:CsnArr  = csnfromarray(array(200, 400))
    surf_mat:CsnArr = csnreshape(csnfromarray(array(60, 40, 30, 60, 40, 30)), shape)
    alph_mat:CsnArr = csnreshape(csnfromarray(array(0.1, 0.25, 0.6, 0.1, 0.25, 0.6)), shape)

    times:CsnArr    = csnt60sab(volumes, surf_mat, alph_mat)
    csnprint times

    ; more absorbent curtains, shorter tail
    dead:CsnArr     = csnfromarray(array(0.1, 0.25, 0.9))
    t60_dead:i      = csnt60sab(200, surfaces, dead)
    prints("T60 (absorbent) : %.4f s\n", t60_dead)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnt60eyr](csnt60eyr.md)
* [csnrt60absp](csnrt60absp.md)
* [csnfschrd](csnfschrd.md)
* [csnfpqr](csnfpqr.md)

## Credits

Pasquale Mainolfi, 2026
