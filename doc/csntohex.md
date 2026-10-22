# csntohex

## Abstract

Writes a whole number as its hexadecimal digits, as Python's `format(n, 'X')`.

## Description

`csntohex` writes a whole number as its hex digits, with no prefix, as
Python's `format(n, 'X')` does: `csntohex(255)` gives `"FF"`. Letters are
upper case, `A` to `F`. `width` is the least number of digits: a shorter
result is padded with `0` on the left (`csntohex(255, 4)` gives `"00FF"`), and
a longer one keeps all its digits. A width of `0` pads nothing, and `0` gives
`"0"`.

The number must be whole, from 0 up to 2^64 excluded, the largest taking 16
digits: a fraction, a negative number, `nan` or `inf` is refused. Csound's
numbers are doubles, so above 2^53 only the numbers a double holds are there.
[csnfromhex](csnfromhex.md) reads the digits back.

A real `CsnArr` gives a `:CsnStr;` of its shape, every number written with the
same width; a value that is refused is named with its index. A complex array
is refused.

The scalar form follows its value's rate: a `k` value takes the k form, which
writes again only when the value changes. The array form takes an explicit
trigger, and the call without one is the init-time form; it runs again only
when the source changed. `width` is init-time, so the k init gives the output,
and every slot of an array, room for the longest number, 16 digits or `width`
if more, and on a [real-time path](csnrtlock.md) a pass writes without
allocating while the source keeps its shape.

## Syntax

```csound
out:S = csntohex(value:i [, width:i])
out:S = csntohex(value:k [, width:i, trig:k])
out:CsnStr = csntohex(source:CsnArr [, width:i])
out:CsnStr = csntohex(source:CsnArr, width:i, trig:k)
```

## Arguments

* `value:i`, `value:k`: a whole number, from 0 up to 2^64 excluded.
* `source:CsnArr`: a real array of such numbers.
* `width:i`: the least number of digits, padded with `0` on the left, optional (default `0`, no padding). The array k form takes it before its trigger.
* `trig:k`: k-rate trigger, optional in the scalar k form (default `1`), which selects the k form of the array one. A zero trigger keeps the previous result.

## Output

* `out:S`: the hex digits of `value`.
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
; csntohex.csd
;
; The three bytes of a MIDI note-on, as hex: status, key and velocity.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    status:S       = csntohex(144)
    prints("status 0x%s\n", status)
    message:i[]    = fillarray(144, 60, 100)
    bytes:CsnArr   = csnfromarray(message)
    text:CsnStr    = csntohex(bytes, 2)
    csnprint(text)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
status 0x90
CsnStr(shape=(3,), dtype=str)
['90' '3C' '64']
```

## See also

* [csnfromhex](csnfromhex.md)
* [csntobinary](csntobinary.md)
* [csnnumtostr](csnnumtostr.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
