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
share shape and dimension count; there is no broadcasting here. Real or
string replacements only — for a complex selection, split the parts with
[csnreal](csnreal.md) and [csnimag](csnimag.md) first.

For the in-place counterpart, which overwrites the mask array itself, see
[csnputmask](csnputmask.md).

The replacements may be strings: with a real mask, a `:CsnStr;` true branch
and a `:CsnStr;` or `S` false branch, the result is a `:CsnStr;` holding its
own copies of the strings. Copying strings allocates, so the k form is refused
on a [real-time path](csnrtlock.md). See [string arrays](string-arrays.md).

## Syntax

```csound
out:CsnArr = csnwhere(mask:CsnArr, on_true:CsnArr, on_false:CsnArr)
out:CsnArr = csnwhere(mask:CsnArr, on_true:CsnArr, on_false:i)
out:CsnArr = csnwhere(mask:CsnArr, on_true:CsnArr, on_false:CsnArr, trig:k)
out:CsnArr = csnwhere(mask:CsnArr, on_true:CsnArr, on_false:k, trig:k)
out:CsnStr = csnwhere(mask:CsnArr, on_true:CsnStr, on_false:CsnStr)
out:CsnStr = csnwhere(mask:CsnArr, on_true:CsnStr, on_false:S)
out:CsnStr = csnwhere(mask:CsnArr, on_true:CsnStr, on_false:CsnStr, trig:k)
out:CsnStr = csnwhere(mask:CsnArr, on_true:CsnStr, on_false:S, trig:k)
```

## Arguments

* `mask:CsnArr`: the selector. Non-zero picks `on_true`, zero picks `on_false`.
* `on_true:CsnArr / on_true:CsnStr`: values taken where the mask is non-zero.
* `on_false:CsnArr / on_false:i / on_false:k / on_false:CsnStr / on_false:S`: values taken where the mask is zero; strings when `on_true` is a `:CsnStr;`.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `out:CsnArr / out:CsnStr`: the selected values.

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
