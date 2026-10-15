# csnstrljust

## Abstract

Elementwise left justification of strings in a width, as `np.strings.ljust`.

## Description

`csnstrljust` pads each string with spaces on the right up to `width`
bytes, so the text sits on the left: `"kick"` in 7 becomes `"kick   "`.
A string already `width` bytes long or longer is kept as it is, and a width
of 0 changes nothing.

The width counts bytes, as `np.strings.ljust` does on a bytes (`S`) array:
`"città"` is 6 bytes long, so it takes one space less than a 5-letter ASCII
word. NumPy on a `str` array counts characters. The width must be a whole
number from 0 up; NumPy also takes a negative one and changes nothing, here it
is refused.

With `fill`, the padding is that character instead of a space: `"ab"` in 4
with `"-"` gives `"ab--"`. It must be exactly one byte, an ASCII character, as
NumPy wants one character in a bytes array; `""`, `"ab"` or a multibyte `"é"`
is refused. The k forms read it on every pass, and a new fill makes the result
again.

Two forms share the name. The one with an output writes a new `:CsnStr;`
holding its own strings and leaves the source alone; the output may also be
the source, `names = csnstrljust(names, 8)`. The one without an output pads the
source in place and returns nothing.

With a k width the k form runs, with a trigger that is optional (default `1`);
a constant or i width with a trigger selects it too. It runs again when the
source, the width or the fill changed. Padding again to the same width changes
nothing, so an output that is its own source keeps its strings from pass to
pass.

Padding makes strings longer. The form with an output pads at init with the
width's value there, an i width or a k width at its init value (`kWidth init
8`), and so gives each element its room: on a [real-time path](csnrtlock.md)
the k form runs while every result fits, a width no larger than at init, on
strings no longer, and stops with a performance error at the first one that
does not. In place, padding cannot be undone, so the init pass pads only with
a width it knows, a constant or an i value, and leaves a k width to the first
pass; on a real-time path every element is checked first, and a pass that
does not fit stops before writing anything. See [string
arrays](string-arrays.md#real-time-paths).

## Syntax

```csound
out:CsnStr = csnstrljust(source:CsnStr, width:i)
out:CsnStr = csnstrljust(source:CsnStr, width:k)
out:CsnStr = csnstrljust(source:CsnStr, width:k, trig:k)
out:CsnStr = csnstrljust(source:CsnStr, width:i, fill:S)
out:CsnStr = csnstrljust(source:CsnStr, width:k, fill:S)
out:CsnStr = csnstrljust(source:CsnStr, width:k, fill:S, trig:k)
csnstrljust(source:CsnStr, width:i)
csnstrljust(source:CsnStr, width:k)
csnstrljust(source:CsnStr, width:k, trig:k)
csnstrljust(source:CsnStr, width:i, fill:S)
csnstrljust(source:CsnStr, width:k, fill:S)
csnstrljust(source:CsnStr, width:k, fill:S, trig:k)
```

## Arguments

* `source:CsnStr`: the strings to pad.
* `width:i / width:k`: the length to pad to, in bytes; a whole number from 0 up.
* `fill:S`: the padding character, exactly one byte; a space when omitted.
* `trig:k`: k-rate trigger, optional (default `1`) with a k width. A zero trigger republishes the previous result.

## Output

* `out:CsnStr`: the justified strings, in the shape of `source`. Omit it for the in-place form.

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
; csnstrljust.csd
;
; Sample names laid out in columns 7 bytes wide, as a copy and then in place,
; and once more padded with dots.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    names:S[]    = fillarray("kick", "snare", "hat")
    raw:CsnStr   = csnfromarray(names)
    cells:CsnStr = csnstrljust(raw, 7)
    csnprint(cells)
    csnstrljust(raw, 7)
    csnprint(raw)
    fresh:CsnStr  = csnfromarray(names)
    marked:CsnStr = csnstrljust(fresh, 7, ".")
    csnprint(marked)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
CsnStr(shape=(3,), dtype=str)
['kick   ' 'snare  ' 'hat    ']
CsnStr(shape=(3,), dtype=str)
['kick   ' 'snare  ' 'hat    ']
CsnStr(shape=(3,), dtype=str)
['kick...' 'snare..' 'hat....']
```

## See also

* [csnstrcenter](csnstrcenter.md)
* [csnstrrjust](csnstrrjust.md)
* [csnstrstrip](csnstrstrip.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
