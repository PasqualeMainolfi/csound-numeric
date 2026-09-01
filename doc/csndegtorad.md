# csndegtorad

## Abstract

Convert every element from degrees to radians.

## Description

`csndegtorad` multiplies every element by `pi / 180`, turning an array of degrees
into the radians the trigonometric opcodes expect.

Real only: an angle in degrees has no complex reading, and a complex array is
refused rather than converted lane by lane.

Two forms share the name. The one with an output publishes a new handle and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing.

## Syntax

```csound
handle:CsnArr = csndegtorad(source:CsnArr)
handle:CsnArr = csndegtorad(source:CsnArr, trig:k)
csndegtorad(source:CsnArr)
csndegtorad(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: the array of degrees.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the array in radians. Omit it for the in-place form.

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
; csndegtorad.csd
;
; The trigonometric opcodes take radians, so this is the step in front of them
; whenever the data arrives in degrees.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    deg:CsnArr   = csnfromarray(array(0, 90, 180, 270, 360))

    rad:CsnArr   = csndegtorad(deg)
    rad_out:i[]  = csntoarray(rad)
    prints("radians = %.4f %.4f %.4f %.4f %.4f\n", rad_out[0], rad_out[1], rad_out[2], rad_out[3], rad_out[4])

    wave:CsnArr  = csnsin(rad)
    wave_out:i[] = csntoarray(wave)
    prints("sine    = %.4f %.4f %.4f %.4f %.4f\n", wave_out[0], wave_out[1], wave_out[2], wave_out[3], wave_out[4])

    ; in place
    csndegtorad(deg)
    now:i[]      = csntoarray(deg)
    prints("in place = %.4f %.4f\n", now[0], now[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnradtodeg](csnradtodeg.md)
* [csnsin](csnsin.md)
* [csnangle](csnangle.md)

## Credits

Pasquale Mainolfi, 2026
