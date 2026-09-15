# csnbincount

## Abstract

Count occurrences of every non-negative integer value, optionally with weights.

## Description

`csnbincount` counts how often each value occurs in a one-dimensional real
array. The value itself selects the output bin: for a source whose largest value
is `m`, the result has `m + 1` elements, and element `i` is the number of source
elements equal to `i`. Bins with no matching element contain zero.

An optional weights array changes the operation from counting occurrences to
summing weights. It must be a one-dimensional real array with exactly the same
length as the source. The weight at position `j` is added to the bin selected by
`source[j]`.

The source must contain only finite, non-negative integer values smaller than
`CSN_MAX_ELEMS` (`2^28`). An empty source returns an empty 1-D array. The result
is always real, including the unweighted form. This follows NumPy's `bincount`,
except that `minlength` is not supported.

## Syntax

```csound
handle:CsnArr = csnbincount(source:CsnArr)
handle:CsnArr = csnbincount(source:CsnArr, weights:CsnArr)
handle:CsnArr = csnbincount(source:CsnArr, trig:k)
handle:CsnArr = csnbincount(source:CsnArr, weights:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: a 1-D real array of finite, non-negative integer values smaller than `2^28`.
* `weights:CsnArr` (optional): a 1-D real array with the same length as `source`.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result. When it is the only k-rate argument, pass it explicitly to select the performance overload.

## Output

* `handle:CsnArr`: a 1-D real array of length `max(source) + 1`, or an empty array when `source` is empty.

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
; csnbincount.csd
;
; Count integer bins, leave gaps at zero, and optionally accumulate one weight
; per source element.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    source:CsnArr     = csnfromarray(array(0, 1, 1, 3))

    counts:CsnArr     = csnbincount(source)
    counts_out:i[]    = csntoarray(counts)
    prints("counts  = %g %g %g %g\n", counts_out[0], counts_out[1], counts_out[2], counts_out[3])

    weights:CsnArr    = csnfromarray(array(0.5, 1, 2, 4))
    weighted:CsnArr   = csnbincount(source, weights)
    weighted_out:i[]  = csntoarray(weighted)
    prints("weighted = %g %g %g %g\n", weighted_out[0], weighted_out[1], weighted_out[2], weighted_out[3])

    empty:CsnArr      = csnbincount(csnempty(array(0)))
    prints("empty size = %d\n", csnsize(empty))
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

The example prints:

```text
counts  = 1 2 0 1
weighted = 0.5 3 0 4
empty size = 0
```

## See also

* [csncnteq](csncnteq.md)
* [csnunique](csnunique.md)
* [csnargwhere](csnargwhere.md)

## Credits

Pasquale Mainolfi, 2026
