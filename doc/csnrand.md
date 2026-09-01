# csnrand

## Abstract

Create an array of uniform random values in a range.

## Description

`csnrand` allocates an array of the requested shape and fills it with values
drawn uniformly from `[min, max)`. The generator is the one owned by the Csound
instance and shared by every `csnrand` in the orchestra; seed it with
[csnseed](csnseed.md) to make a run reproducible.

Real only.

At k-rate the trigger is what keeps a random array *stable*: on a zero trigger
the previous draw is republished unchanged, so a new set of values appears only
when you ask for one.

## Syntax

```csound
handle:CsnArr = csnrand(shape:i[], min:i, max:i)
handle:CsnArr = csnrand(shape:k[], min:k, max:k)
handle:CsnArr = csnrand(shape:k[], min:k, max:k, trig:k)
```

## Arguments

* `shape:i[] / shape:k[]`: one extent per dimension.
* `min:i / min:k`: lower bound, included.
* `max:i / max:k`: upper bound, excluded.
* `trig:k` (optional, default `1`): k-rate trigger. A new draw happens on a non-zero trigger; a zero trigger republishes the previous one.

## Output

* `handle:CsnArr`: handle of the new array.

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
; csnrand.csd
;
; csnrand draws uniformly from [min, max). Seeding first makes the run
; reproducible, which is what lets an example print fixed numbers at all.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    csnseed(12345)

    shape:i[]     = fillarray(6)
    noise:CsnArr  = csnrand(shape, -1, 1)
    lo:i          = csnmin(noise)
    hi:i          = csnmax(noise)
    n:i           = csnsize(noise)
    prints("n = %d, min = %.3f, max = %.3f\n", n, lo, hi)

    in_range:i    = (lo >= -1 && hi < 1 ? 1 : 0)
    prints("all values inside [-1, 1) = %d\n", in_range)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnseed](csnseed.md)
* [csnzeros](csnzeros.md)
* [csnfull](csnfull.md)

## Credits

Pasquale Mainolfi, 2026
