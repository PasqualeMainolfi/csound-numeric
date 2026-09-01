# csnradtodeg

## Abstract

Convert every element from radians to degrees.

## Description

`csnradtodeg` multiplies every element by `180 / pi`. It is the inverse of
[csndegtorad](csndegtorad.md), and the usual last step before printing or
displaying an angle that was computed in radians — by
[csnatan](csnatan.md), [csnangle](csnangle.md) or
[csnangledist](csnangledist.md).

Real only: an angle in degrees has no complex reading, and a complex array is
refused rather than converted lane by lane.

Two forms share the name. The one with an output publishes a new handle and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing.

## Syntax

```csound
handle:CsnArr = csnradtodeg(source:CsnArr)
handle:CsnArr = csnradtodeg(source:CsnArr, trig:k)
csnradtodeg(source:CsnArr)
csnradtodeg(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: the array of radians.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the array in degrees. Omit it for the in-place form.

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
; csnradtodeg.csd
;
; The trigonometric opcodes answer in radians. csnradtodeg is what turns that
; answer into something readable.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    slope:CsnArr = csnfromarray(array(0, 1, -1, 1000))

    ang:CsnArr   = csnatan(slope)
    deg:CsnArr   = csnradtodeg(ang)
    deg_out:i[]  = csntoarray(deg)
    prints("atan in degrees = %.3f %.3f %.3f %.3f\n", deg_out[0], deg_out[1], deg_out[2], deg_out[3])

    ; the round trip
    back:CsnArr  = csndegtorad(deg)
    back_out:i[] = csntoarray(back)
    ang_out:i[]  = csntoarray(ang)
    prints("round trip: %.6f vs %.6f\n", back_out[1], ang_out[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csndegtorad](csndegtorad.md)
* [csnatan](csnatan.md)
* [csnangle](csnangle.md)

## Credits

Pasquale Mainolfi, 2026
