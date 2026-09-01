# csnwrap

## Abstract

Wrap every element into one period, centred on zero.

## Description

`csnwrap` folds every element into a single period by adding or subtracting whole
periods until it lands there. The range is **centred on zero**:
`[-period/2, period/2)`. With `period = 2*pi` an angle comes back in
`[-pi, pi)`, which is the convention [csnangle](csnangle.md) uses too.

It is the phase-domain counterpart of [csnclip](csnclip.md): where clipping
squashes what falls outside a range, wrapping brings it round.

[csnunwrap](csnunwrap.md) is the inverse operation, undoing the jumps a wrap
leaves in a sequence.

Two forms share the name. The one with an output publishes a new handle and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing.

## Syntax

```csound
handle:CsnArr = csnwrap(source:CsnArr, period:i)
handle:CsnArr = csnwrap(source:CsnArr, period:k, trig:k)
csnwrap(source:CsnArr, period:i)
csnwrap(source:CsnArr, period:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to wrap.
* `period:i / period:k`: the period; the result lands in `[-period/2, period/2)`.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the wrapped array. Omit it for the in-place form.

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
; csnwrap.csd
;
; The range is centred on zero, so a 2*pi period brings an angle back into
; [-pi, pi) rather than [0, 2*pi).
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    two_pi:i      = 6.283185307179586
    phase:CsnArr  = csnfromarray(array(-4, -1, 0, 1, 4, 7))

    wrapped:CsnArr = csnwrap(phase, two_pi)
    wrapped_out:i[] = csntoarray(wrapped)
    prints("period 2*pi = %.4f %.4f %.4f %.4f %.4f %.4f\n", wrapped_out[0], wrapped_out[1], wrapped_out[2], wrapped_out[3], wrapped_out[4], wrapped_out[5])

    ; any period works, not only an angular one
    four:CsnArr   = csnwrap(phase, 4)
    four_out:i[]  = csntoarray(four)
    prints("period 4    = %g %g %g %g %g %g\n", four_out[0], four_out[1], four_out[2], four_out[3], four_out[4], four_out[5])

    ; in place
    csnwrap(phase, two_pi)
    now:i[]       = csntoarray(phase)
    prints("in place    = %.4f %.4f\n", now[0], now[5])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnunwrap](csnunwrap.md)
* [csnangle](csnangle.md)
* [csnclip](csnclip.md)
* [csndivmod](csndivmod.md)

## Credits

Pasquale Mainolfi, 2026
