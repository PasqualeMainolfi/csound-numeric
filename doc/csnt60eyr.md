# csnt60eyr

## Abstract

Eyring-Norris reverberation time from a room volume and its absorbing surfaces.

## Description

`csnt60eyr` answers the Eyring-Norris estimate of T60:

```
T60 = -0.161 * V / (S * ln(1 - A/S))
S = sum of the surface areas        A = sum of S_i * alpha_i
```

It takes exactly the arguments [csnt60sab](csnt60sab.md) takes and differs only
in the model. Sabine treats absorption as a continuous loss; Eyring treats it as
a loss suffered once per reflection, and the mean coefficient `A/S` therefore
enters through a logarithm.

The consequence is the whole reason to choose between them. While the mean
coefficient is small the logarithm is nearly linear and the two agree to within
a few percent. As the room gets more absorbent they diverge, and Eyring is the
one that behaves: at a mean coefficient approaching 1 the logarithm sends T60 to
zero, which is what a fully absorbing room does, while Sabine still reports a
tail. Use Sabine for a live room and a handbook number, Eyring for anything
treated.

When the mean coefficient reaches or exceeds 1 — a surface list that absorbs
everything, or coefficients above 1 — the logarithm has no value to give and the
opcode answers `0`, the decay time of a room with no reverberant field. A total
surface or absorption of zero answers `0` for the same reason.

The input shapes, the rates and the k-rate caching are those of
[csnt60sab](csnt60sab.md).

## Syntax

```csound
value:i = csnt60eyr(volume:i, surfaces:CsnArr, alphas:CsnArr)
handle:CsnArr = csnt60eyr(volumes:CsnArr, surfaces:CsnArr, alphas:CsnArr)
value:k = csnt60eyr(volume:k, surfaces:CsnArr, alphas:CsnArr, trig:k)
handle:CsnArr = csnt60eyr(volumes:CsnArr, surfaces:CsnArr, alphas:CsnArr, trig:k)
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
; csnt60eyr.csd
;
; Eyring's reverberation time. Same inputs as csnt60sab, different model: the
; mean absorption coefficient enters through a logarithm, so the two agree in a
; live room and part company as the surfaces get more absorbent. Eyring is the
; one that still answers when the room absorbs nearly everything.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    surfaces:CsnArr = csnfromarray(array(60, 40, 30))
    alphas:CsnArr   = csnfromarray(array(0.1, 0.25, 0.6))

    ; S = 130 m2, A = 34 sabins, mean coefficient 0.2615
    eyring:i        = csnt60eyr(200, surfaces, alphas)
    sabine:i        = csnt60sab(200, surfaces, alphas)
    prints("Eyring          : %.4f s\n", eyring)
    prints("Sabine          : %.4f s\n", sabine)

    ; a live room, mean coefficient 0.05: the two models agree
    live:CsnArr     = csnfromarray(array(0.05, 0.05, 0.05))
    eyring_live:i   = csnt60eyr(200, surfaces, live)
    sabine_live:i   = csnt60sab(200, surfaces, live)
    prints("live  Eyring    : %.4f s\n", eyring_live)
    prints("live  Sabine    : %.4f s\n", sabine_live)

    ; a dead room, mean coefficient 0.85: Sabine still reports a tail that is
    ; not there, Eyring collapses towards zero as the logarithm demands
    dead:CsnArr     = csnfromarray(array(0.85, 0.85, 0.85))
    eyring_dead:i   = csnt60eyr(200, surfaces, dead)
    sabine_dead:i   = csnt60sab(200, surfaces, dead)
    prints("dead  Eyring    : %.4f s\n", eyring_dead)
    prints("dead  Sabine    : %.4f s\n", sabine_dead)

    ; one row per room, as in csnt60sab
    shape:i[]       = fillarray(2, 3)
    volumes:CsnArr  = csnfromarray(array(200, 400))
    surf_mat:CsnArr = csnreshape(csnfromarray(array(60, 40, 30, 60, 40, 30)), shape)
    alph_mat:CsnArr = csnreshape(csnfromarray(array(0.1, 0.25, 0.6, 0.1, 0.25, 0.6)), shape)
    times:CsnArr    = csnt60eyr(volumes, surf_mat, alph_mat)
    csnprint times
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnt60sab](csnt60sab.md)
* [csnrt60absp](csnrt60absp.md)
* [csnfschrd](csnfschrd.md)
* [csnfpqr](csnfpqr.md)

## Credits

Pasquale Mainolfi, 2026
