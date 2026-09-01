# csnunwrap

## Abstract

Remove the jumps left by wrapping, along an axis.

## Description

`csnunwrap` walks a sequence and, wherever the step from one element to the next
exceeds `discont`, adds or subtracts whole periods to bring it back in line — the
inverse of [csnwrap](csnwrap.md), and NumPy's `unwrap`.

The usual arguments are a period of `2*pi` and a discontinuity threshold of
`pi`: a phase sequence that keeps folding back into `[-pi, pi)` comes out as a
continuous curve that can be differentiated, which is how an instantaneous
frequency is recovered from a phase.

The scan follows one axis; `-1`, the default, reads the array flat.

Two forms share the name. The one with an output publishes a new handle and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing.

## Syntax

```csound
handle:CsnArr = csnunwrap(source:CsnArr, period:i, discont:i)
handle:CsnArr = csnunwrap(source:CsnArr, period:i, discont:i, axis:i)
handle:CsnArr = csnunwrap(source:CsnArr, period:k, discont:k, axis:k, trig:k)
csnunwrap(source:CsnArr, period:i, discont:i)
csnunwrap(source:CsnArr, period:i, discont:i, axis:i)
csnunwrap(source:CsnArr, period:k, discont:k, axis:k, trig:k)
```

## Arguments

* `source:CsnArr`: the wrapped sequence.
* `period:i / period:k`: the period to add or subtract, typically `2*pi`.
* `discont:i / discont:k`: the step size above which a jump counts as a wrap, typically `pi`.
* `axis:i / axis:k` (optional, default `-1`): the axis to scan along; `-1` reads the array flat.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the continuous sequence. Omit it for the in-place form.

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
; csnunwrap.csd
;
; A phase that keeps folding back into [-pi, pi) is not differentiable. Unwrap
; it first, then csndiff gives the per-step frequency.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    two_pi:i        = 6.283185307179586
    pi_val:i        = 3.141592653589793

    wrapped:CsnArr  = csnfromarray(array(0, 3, -3, 0, 3))
    cont:CsnArr     = csnunwrap(wrapped, two_pi, pi_val)
    cont_out:i[]    = csntoarray(cont)
    prints("unwrapped = %.4f %.4f %.4f %.4f %.4f\n", cont_out[0], cont_out[1], cont_out[2], cont_out[3], cont_out[4])

    ; now the steps are meaningful
    step:CsnArr     = csndiff(cont)
    step_out:i[]    = csntoarray(step)
    prints("steps     = %.4f %.4f %.4f %.4f\n", step_out[0], step_out[1], step_out[2], step_out[3])

    ; wrapping it again gives the original back
    again:CsnArr    = csnwrap(cont, two_pi)
    again_out:i[]   = csntoarray(again)
    prints("re-wrapped = %.4f %.4f %.4f %.4f %.4f\n", again_out[0], again_out[1], again_out[2], again_out[3], again_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnwrap](csnwrap.md)
* [csndiff](csndiff.md)
* [csnangle](csnangle.md)

## Credits

Pasquale Mainolfi, 2026
