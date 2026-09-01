# csnroll

## Abstract

Circularly shift an array, as a whole or along one axis.

## Description

`csnroll` shifts the elements of an array by `shift` positions; whatever falls
off one end reappears at the other. Nothing is lost and the shape does not
change.

With no axis the array is read flat, so a shift of 1 on `1 2 3 4` gives
`4 1 2 3`. With an axis, each line along that axis is shifted on its own: on a
matrix, `axis = 0` moves the rows and `axis = 1` moves the elements within each
row.

`shift` may be negative, which rolls the other way.

Two forms share the name. The one with an output publishes a new handle and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing.

## Syntax

```csound
handle:CsnArr = csnroll(source:CsnArr, shift:i)
handle:CsnArr = csnroll(source:CsnArr, shift:i, axis:i)
handle:CsnArr = csnroll(source:CsnArr, shift:k)
handle:CsnArr = csnroll(source:CsnArr, shift:k, axis:k)
csnroll(source:CsnArr, shift:i)
csnroll(source:CsnArr, shift:i, axis:i)
csnroll(source:CsnArr, shift:k)
csnroll(source:CsnArr, shift:k, axis:k)
```

## Arguments

* `source:CsnArr`: the array to shift.
* `shift:i / shift:k`: how many positions to shift by; negative rolls the other way.
* `axis:i / axis:k` (optional, default `-1`): the axis to shift along; `-1` reads the array flat.

## Output

* `handle:CsnArr`: handle of the shifted array. Omit it for the in-place form.

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
; csnroll.csd
;
; csnroll shifts and wraps: nothing is lost and the shape is unchanged. Without
; an axis the array is read flat; with one, every line along it moves on its own.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr     = csnfromarray(array(1, 2, 3, 4))

    right:CsnArr   = csnroll(vec, 1)
    right_out:i[]  = csntoarray(right)
    prints("shift  1 = %g %g %g %g\n", right_out[0], right_out[1], right_out[2], right_out[3])

    left:CsnArr    = csnroll(vec, -1)
    left_out:i[]   = csntoarray(left)
    prints("shift -1 = %g %g %g %g\n", left_out[0], left_out[1], left_out[2], left_out[3])

    ; along an axis, each row moves on its own
    shape:i[]      = fillarray(2, 3)
    mat:CsnArr     = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    rolled:CsnArr  = csnroll(mat, 1, 1)
    rolled_out:i[] = csntoarray(csnflatten(rolled))
    prints("axis 1   = %g %g %g %g %g %g\n", rolled_out[0], rolled_out[1], rolled_out[2], rolled_out[3], rolled_out[4], rolled_out[5])

    ; in place
    csnroll(vec, 2)
    now:i[]        = csntoarray(vec)
    prints("in place = %g %g %g %g\n", now[0], now[1], now[2], now[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnflip](csnflip.md)
* [csnreverse](csnreverse.md)
* [csngetslice](csngetslice.md)

## Credits

Pasquale Mainolfi, 2026
