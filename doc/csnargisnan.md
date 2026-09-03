# csnargisnan

## Abstract

Return the coordinates of the NaN elements.

## Description

`csnargisnan` returns the position of every element that is NaN, as a
`matches × ndims` array: one row per element found, each row holding its full
coordinates.

No comparison can find a NaN — the ordered tests and equality are all false
against it, and inequality is true for everything — so this is the only way to
locate them. It is the guard to run over data that came in from a file or a
table, or through a function outside its real domain — `csnsqrt` of a negative
number, say.

Real only. Use [csncntnan](csncntnan.md) when only the count matters.

## Syntax

```csound
handle:CsnArr = csnargisnan(source:CsnArr)
handle:CsnArr = csnargisnan(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: the array to scan.
* `trig:k`: k-rate trigger. The scan is redone on a non-zero trigger; a zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: a `matches × ndims` array of coordinates.

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
; csnargisnan.csd
;
; No comparison can select a NaN, so this is the only way to find one. The real
; square root of a negative number is the easiest way to make some.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr  = csnfromarray(array(4, -1, 9, -16))
    roots:CsnArr = csnsqrt(data)

    bad:CsnArr   = csnargisnan(roots)
    bad_out:i[]  = csntoarray(csnflatten(bad))
    count:i      = csnsize(bad)
    prints("NaN count = %d, at %g and %g\n", count, bad_out[0], bad_out[1])

    ; equality cannot see them
    eq_hits:i    = csncnteq(roots, 0)
    nan_hits:i   = csncntnan(roots)
    prints("elements equal to 0 = %d, NaN elements = %d\n", eq_hits, nan_hits)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnisnan](csnisnan.md)
* [csncntnan](csncntnan.md)
* [csnargnonzero](csnargnonzero.md)
* [csnargwhere](csnargwhere.md)

## Credits

Pasquale Mainolfi, 2026
