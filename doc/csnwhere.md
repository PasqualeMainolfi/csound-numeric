# csnwhere

## Abstract

Elementwise choice between two arrays, driven by a mask.

## Description

`csnwhere` walks a mask array and, for every element, takes the value at the same
position from the first replacement array where the mask is non-zero and from the
second where it is zero. The result is a new array; the operands are left
untouched.

The mask is an ordinary real array, so it is normally the output of a comparison
such as `csngt` or `csneq`, or of a classifier such as
[csnisnan](csnisnan.md), [csnisinf](csnisinf.md), or
[csnisfin](csnisfin.md). They publish exactly the 1.0 and 0.0 pattern this
opcode reads.

The false branch may be a scalar instead of an array. Mask and replacements must
share shape and dimension count; there is no broadcasting here. Real only — for
a complex selection, split the parts with [csnreal](csnreal.md) and
[csnimag](csnimag.md) first.

For the in-place counterpart, which overwrites the mask array itself, see
[csnputmask](csnputmask.md).

## Syntax

```csound
handle:CsnArr = csnwhere(mask:CsnArr, on_true:CsnArr, on_false:CsnArr)
handle:CsnArr = csnwhere(mask:CsnArr, on_true:CsnArr, on_false:i)
handle:CsnArr = csnwhere(mask:CsnArr, on_true:CsnArr, on_false:CsnArr, trig:k)
handle:CsnArr = csnwhere(mask:CsnArr, on_true:CsnArr, on_false:k, trig:k)
```

## Arguments

* `mask:CsnArr`: the selector. Non-zero picks `on_true`, zero picks `on_false`.
* `on_true:CsnArr`: values taken where the mask is non-zero.
* `on_false:CsnArr / on_false:i / on_false:k`: values taken where the mask is zero.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the selected values.

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
; csnwhere.csd
;
; A comparison builds the mask, csnwhere spends it: every element above the
; threshold keeps its value, the others are floored to a constant.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    sig:CsnArr    = csnfromarray(array(0.2, 0.9, 0.4, 1.6))
    gate:CsnArr   = csngt(sig, 0.5)

    kept:CsnArr   = csnwhere(gate, sig, 0)
    kept_out:i[]  = csntoarray(kept)
    prints("gated  = %g %g %g %g\n", kept_out[0], kept_out[1], kept_out[2], kept_out[3])

    ; two arrays: pick from the loud one or the quiet one, position by position
    quiet:CsnArr  = csnmul(sig, 0.1)
    mixed:CsnArr  = csnwhere(gate, sig, quiet)
    mixed_out:i[] = csntoarray(mixed)
    prints("mixed  = %g %g %g %g\n", mixed_out[0], mixed_out[1], mixed_out[2], mixed_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnputmask](csnputmask.md)
* [csncompress](csncompress.md)
* [csnargwhere](csnargwhere.md)
* [csnclip](csnclip.md)

## Credits

Pasquale Mainolfi, 2026
