# csntobinary

## Abstract

Writes a whole number as its binary digits, as Python's `format(n, 'b')`.

## Description

`csntobinary` writes a whole number as its binary digits, with no prefix, as
Python's `format(n, 'b')` does: `csntobinary(5)` gives `"101"`. `width` is the
least number of digits: a shorter result is padded with `0` on the left
(`csntobinary(5, 8)` gives `"00000101"`), and a longer one keeps all its
digits. Without a width, or with `0`, nothing is padded, and `0` gives `"0"`.

The number must be whole, from 0 up to 2^64 excluded, the largest taking 64
digits: a fraction, a negative number, `nan` or `inf` is refused. Csound's
numbers are doubles, so above 2^53 only the numbers a double holds are there.
[csnfrombinary](csnfrombinary.md) reads the digits back.

A real `CsnArr` gives a `:CsnStr;` of its shape, every number written with the
same width; a value that is refused is named with its index. A complex array
is refused.

The scalar form follows its value's rate: a `k` value takes the k form, which
writes again only when the value changes. The array form takes an explicit
trigger, and the call without one is the init-time form; it runs again only
when the source changed. `width` is init-time, so the k init gives the output,
and every slot of an array, room for the longest number, 64 digits or `width`
if more, and on a [real-time path](csnrtlock.md) a pass writes without
allocating while the source keeps its shape.

## Syntax

```csound
out:S = csntobinary(value:i [, width:i])
out:S = csntobinary(value:k [, width:i, trig:k])
out:CsnStr = csntobinary(source:CsnArr [, width:i])
out:CsnStr = csntobinary(source:CsnArr, width:i, trig:k)
```

## Arguments

* `value:i`, `value:k`: a whole number, from 0 up to 2^64 excluded.
* `source:CsnArr`: a real array of such numbers.
* `width:i`: the least number of digits, padded with `0` on the left, optional (default `0`, no padding). The array k form takes it before its trigger.
* `trig:k`: k-rate trigger, optional in the scalar k form (default `1`), which selects the k form of the array one. A zero trigger keeps the previous result.

## Output

* `out:S`: the binary digits of `value`.
* `out:CsnStr`: the digits of every number of `source`, in its shape.

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
; csntobinary.csd
;
; A 16-step drum pattern kept as one number, written out as its steps, and
; the four bit masks of a 4-step bar.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    steps:S        = csntobinary(34952, 16)
    prints("pattern %s\n", steps)
    masks:i[]      = fillarray(1, 2, 4, 8)
    bits:CsnArr    = csnfromarray(masks)
    rows:CsnStr    = csntobinary(bits, 4)
    csnprint(rows)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
pattern 1000100010001000
CsnStr(shape=(4,), dtype=str)
['0001' '0010' '0100' '1000']
```

## See also

* [csnfrombinary](csnfrombinary.md)
* [csntohex](csntohex.md)
* [csnnumtostr](csnnumtostr.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
