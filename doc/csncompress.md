# csncompress

## Abstract

Keeps only the entries a 1-D mask marks, along one axis or over the flattened array.

## Description

`csncompress` selects along a single axis: the mask is a 1-D array read against
that axis, and every position where it is non-zero is kept, in order. With the
axis left at `-1` the source is read flat and the result is 1-D.

The mask may be shorter than the axis it selects on, and then the result is
truncated to the mask's length — the trailing positions are simply never
considered. This is the same rule numpy's `compress` follows. A mask longer than
the axis is an error.

Unlike [csnwhere](csnwhere.md), which keeps the shape and swaps values, this
opcode changes the length of the axis it works on. The mask must be real; the
source may be real or complex.

## Syntax

```csound
handle:CsnArr = csncompress(source:CsnArr, mask:CsnArr)
handle:CsnArr = csncompress(source:CsnArr, mask:CsnArr, axis:i)
handle:CsnArr = csncompress(source:CsnArr, mask:CsnArr, axis:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to select from.
* `mask:CsnArr`: 1-D real mask, no longer than the selected axis.
* `axis:i / axis:k` (optional, default `-1`): axis to select along; `-1` reads the source flat.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the kept entries.

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
; csncompress.csd
;
; Selecting rows out of a matrix, then the same mask read flat over a vector.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(10, 20, 30, 40))
    keep:CsnArr   = csnfromarray(array(1, 0, 1, 1))

    kept:CsnArr   = csncompress(vec, keep)
    kept_out:i[]  = csntoarray(kept)
    prints("kept = %g %g %g\n", kept_out[0], kept_out[1], kept_out[2])

    ; along axis 0 of a 2x2, the mask picks rows
    shape:i[]     = fillarray(2, 2)
    mat:CsnArr    = csnreshape(vec, shape)
    rows:CsnArr   = csnfromarray(array(0, 1))
    row:CsnArr    = csncompress(mat, rows, 0)
    flat:CsnArr   = csnflatten(row)
    row_out:i[]   = csntoarray(flat)
    prints("row  = %g %g\n", row_out[0], row_out[1])
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
* [csnputmask](csnputmask.md)
* [csnargwhere](csnargwhere.md)
* [csntake](csntake.md)

## Credits

Pasquale Mainolfi, 2026
