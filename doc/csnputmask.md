# csnputmask

## Abstract

In-place elementwise choice: the array is its own mask.

## Description

`csnputmask` reads the destination array as a mask and overwrites each of its
elements in the same pass: where the value was non-zero it takes the element from
the first replacement, where it was zero from the second. Nothing is published;
the array named first is modified.

It is the in-place counterpart of [csnwhere](csnwhere.md), and the same shape and
dimension rules apply. Real only.

The operation consumes the mask it reads, so it is not idempotent: after the
first pass the array holds replacement values, not the original 1.0 and 0.0
pattern. At k-rate this is handled by the version check — the opcode runs once
per write made by somebody else, not once per control period — so a re-raised
trigger alone does not apply the mask a second time.

## Syntax

```csound
csnputmask mask:CsnArr, on_true:CsnArr, on_false:CsnArr
csnputmask mask:CsnArr, on_true:CsnArr, on_false:i
csnputmask mask:CsnArr, on_true:CsnArr, on_false:CsnArr, trig:k
csnputmask mask:CsnArr, on_true:CsnArr, on_false:k, trig:k
```

## Arguments

* `mask:CsnArr`: the selector, and the array that is overwritten.
* `on_true:CsnArr`: values taken where the mask is non-zero.
* `on_false:CsnArr / on_false:i / on_false:k`: values taken where the mask is zero.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger skips the pass.

## Output

None. The first argument is modified in place.

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
; csnputmask.csd
;
; The mask array is spent in place: the 1.0 and 0.0 pattern is replaced by the
; values it was selecting, with no second handle allocated.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    sig:CsnArr    = csnfromarray(array(0.2, 0.9, 0.4, 1.6))
    gate:CsnArr   = csngt(sig, 0.5)

    loud:CsnArr   = csnfromarray(array(1, 1, 1, 1))
    csnputmask gate, loud, -1

    gate_out:i[]  = csntoarray(gate)
    prints("mask spent = %g %g %g %g\n", gate_out[0], gate_out[1], gate_out[2], gate_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnwhere](csnwhere.md)
* [csncompress](csncompress.md)
* [csnset](csnset.md)

## Credits

Pasquale Mainolfi, 2026
