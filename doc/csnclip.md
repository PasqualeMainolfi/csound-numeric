# csnclip

## Abstract

Clamp every element between two bounds.

## Description

`csnclip` limits every element to the range `[min, max]`: anything below `min`
becomes `min`, anything above `max` becomes `max`, everything else is left alone.

It is the guard to put in front of an operation with a restricted domain — a
divisor that must stay away from zero, an argument to
[csnasin](csnasin.md) or [csnacos](csnacos.md) that must stay within `[-1, 1]`,
an index that must stay inside an extent.

Real only.

Two forms share the name. The one with an output publishes a new handle and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing.

## Syntax

```csound
handle:CsnArr = csnclip(source:CsnArr, min:i, max:i)
handle:CsnArr = csnclip(source:CsnArr, min:k, max:k)
handle:CsnArr = csnclip(source:CsnArr, min:k, max:k, trig:k)
csnclip(source:CsnArr, min:i, max:i)
csnclip(source:CsnArr, min:k, max:k)
csnclip(source:CsnArr, min:k, max:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to clamp.
* `min:i / min:k`: lower bound.
* `max:i / max:k`: upper bound.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the clamped array. Omit it for the in-place form.

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
; csnclip.csd
;
; csnclip is the guard in front of a restricted domain: keep a divisor off zero,
; keep an arcsine argument inside [-1, 1].
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(-2, -0.5, 0.5, 3))

    unit:CsnArr   = csnclip(data, 0, 1)
    unit_out:i[]  = csntoarray(unit)
    prints("clipped to [0, 1]  = %g %g %g %g\n", unit_out[0], unit_out[1], unit_out[2], unit_out[3])

    ; keep an arcsine argument inside its domain
    safe:CsnArr   = csnclip(data, -1, 1)
    angle:CsnArr  = csnasin(safe)
    angle_out:i[] = csntoarray(angle)
    prints("asin of clamped    = %.4f %.4f %.4f %.4f\n", angle_out[0], angle_out[1], angle_out[2], angle_out[3])

    ; in place
    csnclip(data, -1, 1)
    now:i[]       = csntoarray(data)
    prints("in place           = %g %g %g %g\n", now[0], now[1], now[2], now[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnmin](csnmin.md)
* [csnmax](csnmax.md)
* [csnwrap](csnwrap.md)
* [csndiv](csndiv.md)

## Credits

Pasquale Mainolfi, 2026
