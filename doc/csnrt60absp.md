# csnrt60absp

## Abstract

The absorption a room needs to reach a target reverberation time.

## Description

`csnrt60absp` is [csnt60sab](csnt60sab.md) solved for the absorption instead of
the time:

```
A = 0.161 * V / T60
```

Given a volume in cubic metres and the reverberation time you want in seconds,
it answers the total absorption in sabins the room must have. Subtract what the
room already has — [csnt60sab](csnt60sab.md) with the surfaces you measured — and
what is left is what the treatment has to supply. Divide that by the coefficient
of the material you intend to use and you have the area to cover.

Because the relation is Sabine's, the same caveat applies: the estimate is
reliable while the resulting mean coefficient stays low, and optimistic for a
heavily treated room.

Four overloads cover the four ways the two operands can arrive. Both scalar
gives a scalar. One scalar and one array gives a one-dimensional array of that
array's length. Both arrays gives a two-dimensional result of
`volumes x targets`, every volume paired with every target — the row index
follows the volume, the column index the target.

Real only. A target of zero, or a negative one, answers `0` rather than an
infinity: there is no such room to describe.

The k-rate overloads follow the same four shapes. The scalar/scalar one carries
no trigger — it is two divisions, recomputed every control period — while the
three that publish a handle take a trailing trigger, and a zero trigger
republishes the previous result.

## Syntax

```csound
value:i = csnrt60absp(volume:i, target:i)
handle:CsnArr = csnrt60absp(volume:i, targets:CsnArr)
handle:CsnArr = csnrt60absp(volumes:CsnArr, target:i)
handle:CsnArr = csnrt60absp(volumes:CsnArr, targets:CsnArr)
value:k = csnrt60absp(volume:k, target:k)
handle:CsnArr = csnrt60absp(volume:k, targets:CsnArr, trig:k)
handle:CsnArr = csnrt60absp(volumes:CsnArr, target:k, trig:k)
handle:CsnArr = csnrt60absp(volumes:CsnArr, targets:CsnArr, trig:k)
```

## Arguments

* `volume:i / volume:k`, `volumes:CsnArr`: the room volume in cubic metres, one value or a one-dimensional array of them.
* `target:i / target:k`, `targets:CsnArr`: the reverberation time wanted, in seconds, one value or a one-dimensional array of them.
* `trig:k` (optional, default `1`): k-rate trigger on the overloads that publish a handle. A zero trigger republishes the previous result.

## Output

* `value:i / value:k`: the required absorption in sabins, when both operands are scalars.
* `handle:CsnArr`: one value per element when one operand is an array; a `volumes x targets` matrix when both are.

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
; csnrt60absp.csd
;
; Sabine read backwards: given a volume and the reverberation time you want,
; how much absorption does the room need? A = 0.161 V / T60, in sabins. Either
; side may be an array; when both are, every volume is paired with every target
; and the result is a volumes x targets matrix.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; one room, one target
    absorption:i     = csnrt60absp(200, 0.8)
    prints("200 m3 -> 0.8 s : %.3f sabins\n", absorption)

    ; one room, a range of targets: a drier room needs more absorption
    targets:CsnArr   = csnfromarray(array(0.6, 0.8, 1.2, 2.0))
    needed:CsnArr    = csnrt60absp(200, targets)
    csnprint needed

    ; several rooms, one target: a bigger room needs proportionally more
    volumes:CsnArr   = csnfromarray(array(100, 200, 400))
    per_room:CsnArr  = csnrt60absp(volumes, 0.8)
    csnprint per_room

    ; both as arrays: row per volume, column per target
    grid:CsnArr      = csnrt60absp(volumes, targets)
    csnprint grid

    ; what is missing from a room that has 20 sabins today
    have:i           = 20
    short:i          = absorption - have
    prints("still missing   : %.3f sabins\n", short)
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
* [csnt60eyr](csnt60eyr.md)
* [csnfschrd](csnfschrd.md)

## Credits

Pasquale Mainolfi, 2026
