# csnstrrstrip

## Abstract

Elementwise removal of whitespace at the end of strings, as `np.strings.rstrip`, or on one string as Python's `str.rstrip`.

## Description

`csnstrrstrip` removes the whitespace at the end of each string, leaving the
start as it is: `"  kick "` becomes `"  kick"`. A string made only of whitespace
becomes `""`. The whitespace is the six ASCII characters Python strips from
bytes: space, tab, newline, vertical tab, form feed and carriage return.

Nothing else counts as whitespace, whatever the locale. This is what
`np.strings.rstrip` does on a bytes (`S`) array, and it keeps UTF-8 safe: the
byte 0xA0 that ends `à` is never taken for a no-break space, so `"città "`
gives `"città"`. NumPy on a `str` array also strips the Unicode spaces, such
as the no-break space.

With `chars`, the bytes in that string are removed at the end in place of
whitespace, in any order and any number, as in Python: `"xyhixy"` with
`"xy"` gives `"xyhi"`.
An empty `chars` removes nothing. The set is of bytes, as on a NumPy bytes
array, so a multibyte character in it stands for each of its bytes: `"é"`
(0xC3 0xA9) removes either byte alone, and can split a character that shares
one, such as `"©"` (0xC2 0xA9) or `"à"` (0xC3 0xA0). The k forms read
`chars` on every pass.

Two forms share the name. The one with an output writes a new `:CsnStr;`
holding its own strings and leaves the source alone; the output may also be
the source, `names = csnstrrstrip(names)`. The one without an output rewrites
the source in place and returns nothing.

The k forms take an explicit trigger, and the call without one is the
init-time form. They run again only when the source or `chars` changed:
stripping twice gives what stripping once gave.

A string never gets longer, so the in-place form writes into the buffers its
elements already hold and allocates nothing, and may run on a [real-time
path](csnrtlock.md). The form with an output writes each string into the room
its element already holds: on a real-time path the init pass gives a marked
output room for the source's longest string, and the k form stops with a
performance error at the first string that does not fit. See [string
arrays](string-arrays.md#real-time-paths).

On one `S` it returns an `S`, as Python's `str.rstrip`, with the same rules for
whitespace and `chars`: `csnstrrstrip(Sname)` or `csnstrrstrip(Sname, "-")`. It
has no in-place form, as a string argument is an input (a literal among
them): `Sname = csnstrrstrip(Sname)` strips `Sname`. Its k form takes a
trigger, strips again on every pass that runs, and computes at init too;
like `strcpyk`, its output takes the room it needs at perf, also on a
real-time path.

## Syntax

```csound
out:CsnStr = csnstrrstrip(source:CsnStr)
out:CsnStr = csnstrrstrip(source:CsnStr, trig:k)
out:CsnStr = csnstrrstrip(source:CsnStr, chars:S)
out:CsnStr = csnstrrstrip(source:CsnStr, chars:S, trig:k)
csnstrrstrip(source:CsnStr)
csnstrrstrip(source:CsnStr, trig:k)
csnstrrstrip(source:CsnStr, chars:S)
csnstrrstrip(source:CsnStr, chars:S, trig:k)
out:S = csnstrrstrip(source:S)
out:S = csnstrrstrip(source:S, trig:k)
out:S = csnstrrstrip(source:S, chars:S)
out:S = csnstrrstrip(source:S, chars:S, trig:k)
```

## Arguments

* `source:CsnStr`: the strings to strip; or `source:S`, one string.
* `chars:S`: the bytes to remove in place of whitespace. Read on every pass by the k forms.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger republishes the previous result.

## Output

* `out:CsnStr`: the stripped strings, in the shape of `source`. Omit it for the in-place form.
* `out:S`: the stripped string, for an `S` source.

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
; csnstrrstrip.csd
;
; Sample names read with stray spaces around them, cleaned as a copy and then
; in place, and tags marked with dashes and underscores, cleaned with chars.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    names:S[]    = fillarray("  kick ", " snare  ", "hat")
    raw:CsnStr   = csnfromarray(names)
    clean:CsnStr = csnstrrstrip(raw)
    csnprint(clean)
    csnstrrstrip(raw)
    csnprint(raw)
    tags:S[]      = fillarray("--kick_", "_snare--", "hat")
    marked:CsnStr = csnfromarray(tags)
    bare:CsnStr   = csnstrrstrip(marked, "-_")
    csnprint(bare)
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
['  kick' ' snare' 'hat']
CsnStr(shape=(3,), dtype=str)
['  kick' ' snare' 'hat']
CsnStr(shape=(3,), dtype=str)
['--kick' '_snare' 'hat']
```

## See also

* [csnstrstrip](csnstrstrip.md)
* [csnstrlstrip](csnstrlstrip.md)
* [csnstradd](csnstradd.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
