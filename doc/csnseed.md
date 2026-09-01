# csnseed

## Abstract

Seed the random generator used by `csnrand`.

## Description

`csnseed` sets the state of the pseudo-random generator owned by the Csound
instance, the one [csnrand](csnrand.md) draws from. Seeding with the same value
makes every subsequent draw reproducible, which is what a regression test or a
documented example needs.

The generator is shared by the whole orchestra, so a seed set in one instrument
affects the `csnrand` calls of every other. Run it at init, once, before the
draws you want to pin down.

## Syntax

```csound
csnseed(seed:i)
```

## Arguments

* `seed:i`: the seed value.

## Output

None.

## Execution Time

* Init

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnseed.csd
;
; csnseed pins the generator down. Re-seeding with the same value before a
; second draw reproduces the first one exactly.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]     = fillarray(4)

    csnseed(2718)
    first:CsnArr  = csnrand(shape, 0, 1)
    first_out:i[] = csntoarray(first)

    csnseed(2718)
    again:CsnArr  = csnrand(shape, 0, 1)
    again_out:i[] = csntoarray(again)

    prints("first  = %.4f %.4f %.4f %.4f\n", first_out[0], first_out[1], first_out[2], first_out[3])
    prints("again  = %.4f %.4f %.4f %.4f\n", again_out[0], again_out[1], again_out[2], again_out[3])

    diff:CsnArr   = csnsubtract(first, again)
    abs_sum:i     = csnsum(csnabs(diff))
    identical:i   = (abs_sum == 0 ? 1 : 0)
    prints("identical = %d\n", identical)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnrand](csnrand.md)

## Credits

Pasquale Mainolfi, 2026
