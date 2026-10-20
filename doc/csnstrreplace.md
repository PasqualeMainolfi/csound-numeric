# csnstrreplace

## Abstract

Elementwise replacement of a substring, as `np.strings.replace`, or on one
string as Python's `str.replace`.

## Description

`csnstrreplace` replaces the occurrences of `old` in each string with `new`:
`"a-b-c"` with `"-"` and `"+"` becomes `"a+b+c"`. The occurrences are found
from the left and never overlap, so `"aaa"` with `"aa"` and `"x"` gives
`"xa"`, and the text put in is never searched again. With `count`, only the
first `count` occurrences are replaced; a negative count, the default `-1`,
replaces them all, and `0` none, as in Python.

An empty `old` matches before every byte and at the end: `"aaa"` with `""`
and `"|"` gives `"|a|a|a|"`. The strings are bytes, as `np.strings.replace`
treats a bytes (`S`) array: an `old` that is a whole UTF-8 character replaces
that character, but an empty `old` puts `new` between the bytes of a
multibyte one, as NumPy does.

Two forms share the name. The one with an output writes a new `:CsnStr;`
holding its own strings and leaves the source alone. The one without an
output replaces in place and returns nothing.

On one `S` it returns an `S`, as Python's `str.replace`:
`csnstrreplace("a-b", "-", "+")` gives `"a+b"`, with the same rules for
`count` and an empty `old`. It has no in-place form, as a string argument is
an input (a literal among them): `Sx = csnstrreplace(Sx, "-", "+")` replaces
in `Sx`. Its k form takes `count` and a trigger, replaces again on every pass
that runs, and computes at init too. Like `strcpyk`, its output grows at perf
as it needs, also on a [real-time path](csnrtlock.md): an `S` has no slot to
reserve.

The k forms take `count` and a trigger, which is optional (default `1`):
`csnstrreplace(names, Sold, Snew, -1)` is the init-time form,
`csnstrreplace(names, Sold, Snew, -1, kTrig)` the k form. They read `old` and
`new` on every pass and run again when the source, `old` or `new` changed.
Replacing is not idempotent (`"a"` with `"aa"` grows the string again), so an
output that is its own source, `names = csnstrreplace(names, "a", "aa", -1,
kTrig)`, replaces again on every pass that runs, and the in-place k form once
for each change.

A replacement can make strings longer. The form with an output computes at
init, giving each element the room of its result there: on a [real-time
path](csnrtlock.md) the k form runs while every result fits and stops with a
performance error at the first one that does not. The in-place form checks
every element first, so a pass that does not fit stops before writing
anything. See [string arrays](string-arrays.md#real-time-paths).

## Syntax

```csound
out:CsnStr = csnstrreplace(source:CsnStr, old:S, new:S [, count:i])
out:CsnStr = csnstrreplace(source:CsnStr, old:S, new:S, count:i, trig:k)
csnstrreplace(source:CsnStr, old:S, new:S [, count:i])
csnstrreplace(source:CsnStr, old:S, new:S, count:i, trig:k)
out:S = csnstrreplace(source:S, old:S, new:S [, count:i])
out:S = csnstrreplace(source:S, old:S, new:S, count:i, trig:k)
```

## Arguments

* `source:CsnStr`: the strings to search; or `source:S`, one string.
* `old:S`: the substring to replace; `""` matches between the bytes.
* `new:S`: what each occurrence becomes.
* `count:i`: how many occurrences to replace in each string, from the left; a whole number, negative for all (default `-1`).
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger republishes the previous result.

## Output

* `out:CsnStr`: the strings after the replacement, in the shape of `source`. Omit it for the in-place form.
* `out:S`: the string after the replacement, for an `S` source.

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
; csnstrreplace.csd
;
; Sample paths moved to another folder as a copy, then the separators changed
; in place, the first one only; last, the same on one path.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    paths:S[]    = fillarray("drums/kick.wav", "drums/snare.wav", "fx/rise.wav")
    raw:CsnStr   = csnfromarray(paths)
    moved:CsnStr = csnstrreplace(raw, "drums/", "kit/")
    csnprint(moved)
    csnstrreplace(raw, "/", "_", 1)
    csnprint(raw)
    one:S        = csnstrreplace("drums/kick.wav", "drums/", "kit/")
    prints("%s\n", one)
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
['kit/kick.wav' 'kit/snare.wav' 'fx/rise.wav']
CsnStr(shape=(3,), dtype=str)
['drums_kick.wav' 'drums_snare.wav' 'fx_rise.wav']
kit/kick.wav
```

## See also

* [csnstrstrip](csnstrstrip.md)
* [csnstradd](csnstradd.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
