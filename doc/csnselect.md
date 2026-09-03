# csnselect

## Abstract

Keeps the elements a mask marks, dropping the rest.

## Description

`csnselect` walks the source and the mask together and copies every element whose
mask cell is non-zero. The result is always 1-D, however many dimensions the
source has, and as long as the number of marks: it is a gather, not a selection
that preserves the layout.

The mask must have the same shape and dimension count as the source — there is no
broadcasting — and must be real. The source may be real or complex.

The natural way to build a mask is a comparison such as `csngt` or `csneq`, or
a classifier such as [csnisnan](csnisnan.md), [csnisinf](csnisinf.md), or
[csnisfin](csnisfin.md). They publish exactly the 1.0 and 0.0 pattern this
opcode reads.

Three neighbours do related jobs. [csnwhere](csnwhere.md) keeps the shape and
swaps values instead of dropping them. [csncompress](csncompress.md) selects
along one axis with a 1-D mask, so it keeps the rank. [csnargwhere](csnargwhere.md)
returns the coordinates of the matches rather than the values.

## Syntax

```csound
handle:CsnArr = csnselect(source:CsnArr, mask:CsnArr)
handle:CsnArr = csnselect(source:CsnArr, mask:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: the array to gather from.
* `mask:CsnArr`: real mask, same shape and dimension count as the source. Non-zero keeps the element.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: 1-D handle of the kept elements, in source order.

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
; csnselect.csd
;
; A mask keeps the elements it marks and drops the rest. The result is always
; 1-D and as long as the number of marks, whatever the shape of the source.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(10, 20, 30, 40))
    mask:CsnArr   = csnfromarray(array(0, 1, 1, 0))

    kept:CsnArr   = csnselect(vec, mask)
    kept_out:i[]  = csntoarray(kept)
    prints("kept   = %g %g\n", kept_out[0], kept_out[1])

    ; the usual source of a mask is a comparison
    sig:CsnArr    = csnfromarray(array(0.2, 0.9, 0.4, 1.6))
    loud:CsnArr   = csngt(sig, 0.5)
    peaks:CsnArr  = csnselect(sig, loud)
    peaks_out:i[] = csntoarray(peaks)
    n:i           = csnsize(peaks)
    prints("above  = %d values: %g %g\n", n, peaks_out[0], peaks_out[1])

    ; a 2-D source needs a 2-D mask of the same shape; the result is flat
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    corners:CsnArr = csnreshape(csnfromarray(array(1, 0, 0, 0, 0, 1)), shape)
    picked:CsnArr = csnselect(mat, corners)
    picked_out:i[] = csntoarray(picked)
    prints("corners = %g %g\n", picked_out[0], picked_out[1])
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
* [csnargwhere](csnargwhere.md)
* [csnputmask](csnputmask.md)

## Credits

Pasquale Mainolfi, 2026
