# csncntnan

## Abstract

Count the NaN elements.

## Description

`csncntnan` returns how many elements of an array are NaN.

No comparison can find a NaN — the ordered tests and equality are all false
against it, and inequality is true for everything — so this and
[csnargisnan](csnargisnan.md) are the only ways to see them. Use this one when
only the count matters, as a cheap validity check over data that came in from a
file or through a function outside its real domain.

Real only.

## Syntax

```csound
count:i = csncntnan(source:CsnArr)
count:k = csncntnan(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: the array to scan.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous count.

## Output

* `count:i / count:k`: how many elements are NaN.

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
; csncntnan.csd
;
; A cheap validity check: run it after anything that can leave the real domain,
; before the statistics that a NaN would poison.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr  = csnfromarray(array(4, -1, 9, -16))
    roots:CsnArr = csnsqrt(data)

    bad:i        = csncntnan(roots)
    prints("NaN elements = %d\n", bad)

    ; a NaN propagates through the reductions, so check before trusting them
    if bad == 0 then
        avg:i = csnmean(roots)
        prints("mean = %g\n", avg)
    else
        prints("skipping the mean: %d NaN elements\n", bad)
        safe:CsnArr = csnsqrt(csnclip(data, 0, 1000))
        avg2:i      = csnmean(safe)
        prints("mean after clipping = %g\n", avg2)
    endif
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnargisnan](csnargisnan.md)
* [csncntnz](csncntnz.md)
* [csncnteq](csncnteq.md)

## Credits

Pasquale Mainolfi, 2026
