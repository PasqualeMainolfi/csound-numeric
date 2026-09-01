# csnsort

## Abstract

Sort an array, over everything or along an axis.

## Description

`csnsort` puts the elements in ascending order. With no axis the array is read
flat and sorted as one run; given an axis each line along it is sorted on its
own, so a matrix sorted on axis 1 has every row in order while the rows stay
where they are.

Duplicates are kept — nothing is collapsed; [csnunique](csnunique.md) is what
does that. To know *where* the sorted elements came from, use
[csnargsort](csnargsort.md).

Real only — ordering has no meaning over the complex field.

Two forms share the name. The one with an output publishes a new handle and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing.

## Syntax

```csound
handle:CsnArr = csnsort(source:CsnArr)
handle:CsnArr = csnsort(source:CsnArr, axis:i)
handle:CsnArr = csnsort(source:CsnArr, axis:k)
handle:CsnArr = csnsort(source:CsnArr, axis:k, trig:k)
csnsort(source:CsnArr)
csnsort(source:CsnArr, axis:i)
csnsort(source:CsnArr, axis:k)
csnsort(source:CsnArr, axis:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to sort.
* `axis:i / axis:k` (optional, default `-1`): the axis to sort along; `-1` reads the array flat.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the sorted array. Omit it for the in-place form.

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
; csnsort.csd
;
; Ascending order, duplicates kept. With an axis every row is sorted on its own
; and the rows stay where they are.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(3, 1, 4, 1, 5))

    sorted:CsnArr  = csnsort(data)
    sorted_out:i[] = csntoarray(sorted)
    prints("sorted = %g %g %g %g %g\n", sorted_out[0], sorted_out[1], sorted_out[2], sorted_out[3], sorted_out[4])

    ; the source is untouched
    src_out:i[]    = csntoarray(data)
    prints("source = %g %g %g %g %g\n", src_out[0], src_out[1], src_out[2], src_out[3], src_out[4])

    ; along an axis: every row sorted, rows in place
    shape:i[]      = fillarray(2, 3)
    mat:CsnArr     = csnreshape(csnfromarray(array(3, 1, 2, 9, 7, 8)), shape)
    rows:CsnArr    = csnsort(mat, 1)
    rows_out:i[]   = csntoarray(csnflatten(rows))
    prints("per row = %g %g %g | %g %g %g\n", rows_out[0], rows_out[1], rows_out[2], rows_out[3], rows_out[4], rows_out[5])

    ; in place
    csnsort(data)
    now:i[]        = csntoarray(data)
    prints("in place = %g %g %g %g %g\n", now[0], now[1], now[2], now[3], now[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnargsort](csnargsort.md)
* [csnunique](csnunique.md)
* [csnmedian](csnmedian.md)
* [csnpercentile](csnpercentile.md)

## Credits

Pasquale Mainolfi, 2026
