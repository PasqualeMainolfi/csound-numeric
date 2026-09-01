# csnge

## Abstract

Elementwise greater than or equal, as a 0/1 array.

## Description

`csnge` compares every element against a scalar and returns an array of the same
shape holding `1` where the test passed and `0` where it did not — a mask, not a
selection.

The mask is what the rest of the suite consumes: multiply by it with
[csnmul](csnmul.md) to zero out what failed, count the hits with
[csnsum](csnsum.md), locate them with [csnargnonzero](csnargnonzero.md), or
combine two of them with [csnlogicand](csnlogicand.md) and
[csnlogicor](csnlogicor.md).

Real only. A NaN fails every ordered comparison, so it never appears in the mask;
[csnargisnan](csnargisnan.md) is the way to find one.

## Syntax

```csound
handle:CsnArr = csnge(source:CsnArr, value:i)
handle:CsnArr = csnge(source:CsnArr, value:k)
handle:CsnArr = csnge(source:CsnArr, value:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to test.
* `value:i / value:k`: the value every element is compared against.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: a 0/1 array with the shape of the source.

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
; csnge.csd
;
; The result is a mask, not a selection. Multiply by it to gate, sum it to
; count, csnargnonzero it to get the positions.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(-2, 0, 1, 3, 5))

    mask:CsnArr   = csnge(data, 1)
    mask_out:i[]  = csntoarray(mask)
    prints("mask  = %g %g %g %g %g\n", mask_out[0], mask_out[1], mask_out[2], mask_out[3], mask_out[4])

    hits:i        = csnsum(mask)
    prints("hits  = %g\n", hits)

    ; gate the data with its own mask
    gated:CsnArr  = csnmul(data, mask)
    gated_out:i[] = csntoarray(gated)
    prints("gated = %g %g %g %g %g\n", gated_out[0], gated_out[1], gated_out[2], gated_out[3], gated_out[4])

    ; and the positions that passed
    where:CsnArr  = csnargnonzero(mask)
    where_n:i     = csnsize(where)
    prints("passing positions = %d\n", where_n)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csngt](csngt.md)
* [csnle](csnle.md)
* [csnlt](csnlt.md)
* [csnargnonzero](csnargnonzero.md)

## Credits

Pasquale Mainolfi, 2026
