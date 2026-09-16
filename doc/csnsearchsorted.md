# csnsearchsorted

## Abstract

Find insertion indices in an ascending one-dimensional array.

## Description

`csnsearchsorted` finds the positions where one or more values can be inserted
into `source` without changing its ascending order. `source` must already be a
one-dimensional real array in ascending order; the opcode does not sort it or
verify that ordering.

By default (`side = 0`, left), an exact match is placed before the matching run.
With `side = 1` (right), it is placed after the matching run. Values below or
above the source range return `0` or `csnsize(source)`, respectively. An empty
source therefore returns zero for every query, while an empty query array
returns an empty one-dimensional result.

The array form accepts a one-dimensional real array of query values and returns
one index for each query. The scalar form returns one i- or k-rate index. The
comparison order places NaN after positive infinity, matching NumPy's enhanced
sort order. The result is equivalent to NumPy's `searchsorted`; NumPy's optional
`sorter` argument is not supported.

## Syntax

```csound
indices:CsnArr = csnsearchsorted(source:CsnArr, values:CsnArr)
indices:CsnArr = csnsearchsorted(source:CsnArr, values:CsnArr, side:i)
indices:CsnArr = csnsearchsorted(source:CsnArr, values:CsnArr, side:i, trig:k)

index:i = csnsearchsorted(source:CsnArr, value:i)
index:i = csnsearchsorted(source:CsnArr, value:i, side:i)
index:k = csnsearchsorted(source:CsnArr, value:k)
index:k = csnsearchsorted(source:CsnArr, value:k, side:i)
index:k = csnsearchsorted(source:CsnArr, value:k, side:i, trig:k)
```

## Arguments

* `source:CsnArr`: an ascending 1-D real array. Its ordering is a caller precondition.
* `values:CsnArr`: a 1-D real array containing the values to locate.
* `value:i / value:k`: one value to locate.
* `side:i` (optional, default `0`): `0` returns the first suitable index (left); `1` returns the last suitable index (right).
* `trig:k` (optional, default `1`): k-rate trigger. Zero retains the result computed by the init pass. The array form needs an explicit trigger to select its performance overload because its other inputs are init-rate handles and values.

## Output

* `indices:CsnArr`: a 1-D real array with one insertion index per element of `values`.
* `index:i / index:k`: the insertion index for a scalar query.

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
; csnsearchsorted.csd
;
; Find insertion points on either side of a duplicate run. The source is
; already sorted; searchsorted never rearranges or validates it.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    sorted:CsnArr    = csnfromarray(array(1, 3, 3, 5))
    wanted:CsnArr    = csnfromarray(array(0, 3, 4, 6))

    left:CsnArr      = csnsearchsorted(sorted, wanted)
    right:CsnArr     = csnsearchsorted(sorted, wanted, 1)
    left_out:i[]     = csntoarray(left)
    right_out:i[]    = csntoarray(right)
    prints("left  = %g %g %g %g\n", left_out[0], left_out[1], left_out[2], left_out[3])
    prints("right = %g %g %g %g\n", right_out[0], right_out[1], right_out[2], right_out[3])

    left_three:i     = csnsearchsorted(sorted, 3)
    right_three:i    = csnsearchsorted(sorted, 3, 1)
    prints("3 spans insertion indices %g through %g\n", left_three, right_three)
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
left  = 0 1 3 4
right = 0 3 3 4
3 spans insertion indices 1 through 3
```

## See also

* [csnsort](csnsort.md)
* [csnindexof](csnindexof.md)
* [csnargsort](csnargsort.md)
* [csnsetcontains](csnsetcontains.md)

## Credits

Pasquale Mainolfi, 2026
