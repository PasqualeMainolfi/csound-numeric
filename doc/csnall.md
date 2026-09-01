# csnall

## Abstract

Return 1 when every element is non-zero.

## Description

`csnall` reads its input as truth values and reports whether they are *all* true.
Over the whole array it returns a single number, `1` or `0`. Given an axis it
reduces along that axis instead and returns an array: one answer per line, with
the reduced axis dropped.

Its usual input is a mask from the comparison opcodes: `csnall(csngt(data, 0))`
answers "is every element positive".

An empty array gives `1`, the neutral answer for a universal test over nothing.
Both real and complex arrays are accepted; a complex element is true when either
lane is non-zero.

## Syntax

```csound
result:i = csnall(source:CsnArr)
result:k = csnall(source:CsnArr)
result:k = csnall(source:CsnArr, trig:k)
handle:CsnArr = csnall(source:CsnArr, axis:i)
handle:CsnArr = csnall(source:CsnArr, axis:k)
handle:CsnArr = csnall(source:CsnArr, axis:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to test, read as truth values.
* `axis:i / axis:k` (optional): the axis to reduce along. Omitted, the whole array is reduced to one number.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `result:i / result:k`: `1` when every element is non-zero, without an axis.
* `handle:CsnArr`: one answer per line along `axis`, with that axis dropped.

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
; csnall.csd
;
; Compare, then csnall: "is every element positive" is one line. With an axis it
; answers per row or per column instead.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(1, 2, 3, 4))
    positive:i     = csnall(csngt(data, 0))
    over_two:i     = csnall(csngt(data, 2))
    prints("all > 0 = %d, all > 2 = %d\n", positive, over_two)

    ; per row: the mask of a 2 x 3 reduced along axis 1
    shape:i[]      = fillarray(2, 3)
    mat:CsnArr     = csnreshape(csnfromarray(array(1, 2, 3, 0, 5, 6)), shape)
    mask:CsnArr    = csngt(mat, 0)
    rows:CsnArr    = csnall(mask, 1)
    rows_out:i[]   = csntoarray(rows)
    prints("row 0 all positive = %g, row 1 = %g\n", rows_out[0], rows_out[1])

    ; the neutral answer over nothing
    cap:i[]        = fillarray(4)
    nothing:CsnArr = csnempty(cap)
    over_empty:i   = csnall(nothing)
    prints("over an empty array = %d\n", over_empty)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnany](csnany.md)
* [csnlogicand](csnlogicand.md)
* [csngt](csngt.md)
* [csncntnz](csncntnz.md)

## Credits

Pasquale Mainolfi, 2026
