# csnany

## Abstract

Return 1 when at least one element is non-zero.

## Description

`csnany` reads its input as truth values and reports whether *any* of them is
true. Over the whole array it returns a single number, `1` or `0`. Given an axis
it reduces along that axis instead and returns an array: one answer per line, with
the reduced axis dropped.

It is the existential counterpart of [csnall](csnall.md), and its usual input is
likewise a mask: `csnany(csngt(data, 1))` answers "did anything clip".

An empty array gives `0`, the neutral answer for an existential test over nothing.
Both real and complex arrays are accepted; a complex element is true when either
lane is non-zero.

## Syntax

```csound
result:i = csnany(source:CsnArr)
result:k = csnany(source:CsnArr)
result:k = csnany(source:CsnArr, trig:k)
handle:CsnArr = csnany(source:CsnArr, axis:i)
handle:CsnArr = csnany(source:CsnArr, axis:k)
handle:CsnArr = csnany(source:CsnArr, axis:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to test, read as truth values.
* `axis:i / axis:k` (optional): the axis to reduce along. Omitted, the whole array is reduced to one number.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `result:i / result:k`: `1` when at least one element is non-zero, without an axis.
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
; csnany.csd
;
; "Did anything clip" is a comparison and a csnany. With an axis the same
; question is answered per row.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    signal:CsnArr  = csnfromarray(array(0.2, 0.9, 1.4, 0.1))
    clipped:i      = csnany(csngt(signal, 1))
    silent:i       = csnany(csneq(signal, 0))
    prints("anything above 1 = %d, any exact zero = %d\n", clipped, silent)

    ; per row
    shape:i[]      = fillarray(2, 3)
    mat:CsnArr     = csnreshape(csnfromarray(array(0, 0, 0, 0, 5, 0)), shape)
    rows:CsnArr    = csnany(mat, 1)
    rows_out:i[]   = csntoarray(rows)
    prints("row 0 has a non-zero = %g, row 1 = %g\n", rows_out[0], rows_out[1])

    ; the neutral answer over nothing
    cap:i[]        = fillarray(4)
    nothing:CsnArr = csnempty(cap)
    over_empty:i   = csnany(nothing)
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

* [csnall](csnall.md)
* [csnlogicor](csnlogicor.md)
* [csncntnz](csncntnz.md)

## Credits

Pasquale Mainolfi, 2026
