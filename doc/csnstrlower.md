# csnstrlower

## Abstract

Elementwise conversion of strings to lower case, as `np.strings.lower`.

## Description

`csnstrlower` turns every capital of each string into its lower-case letter
(`A`-`Z` to `a`-`z`). Only the ASCII letters change: every byte from 0x80 up
is left as it is, whatever the locale, so a UTF-8 string keeps its multibyte
characters (`"Détaché"` keeps its `é`). This is what `np.strings.lower` does
on a bytes (`S`) array; on a `str` array NumPy maps the other Unicode letters
too.

Two forms share the name. The one with an output writes a new `:CsnStr;`
holding its own strings and leaves the source alone; the output may also be
the source, `notes = csnstrlower(notes)`. The one without an output rewrites
the source in place and returns nothing.

The k forms take an explicit trigger, and the call without one is the
init-time form. They run again only when the source changed: converting twice
gives what converting once gave.

A string keeps its length, so the in-place form writes into the buffers its
elements already hold and allocates nothing, and may run on a [real-time
path](csnrtlock.md). The form with an output writes each string into the room
its element already holds: on a real-time path the init pass gives a marked
output room for the source's longest string, and the k form stops with a
performance error at the first string that does not fit. See [string
arrays](string-arrays.md#real-time-paths).

On one `S` it returns an `S`: `csnstrlower("Kick 1")` gives "kick 1". It has
no in-place form, as a string argument is an input: `Sx = csnstrlower(Sx)`
converts `Sx`. An `S` has no rate, so the k form takes a trigger, and computes
at init too. Like `strcpyk`, its output takes the room it needs at perf.

## Syntax

```csound
out:CsnStr = csnstrlower(source:CsnStr)
out:CsnStr = csnstrlower(source:CsnStr, trig:k)
csnstrlower(source:CsnStr)
csnstrlower(source:CsnStr, trig:k)
out:S = csnstrlower(source:S)
out:S = csnstrlower(source:S, trig:k)
```

## Arguments

* `source:CsnStr`: the strings to convert; or `source:S`, one string.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger republishes the previous result.

## Output

* `out:CsnStr`: the converted strings, in the shape of `source`. Omit it for the in-place form.
* `out:S`: the converted string, for an `S` source.

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
; csnstrlower.csd
;
; Note names in lower case as a copy, then the copy in capitals in place.
; Only ASCII letters change: the accented name keeps its "é".
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    names:S[]     = fillarray("c4", "Eb4", "g#4", "Détaché")
    notes:CsnStr  = csnfromarray(names)
    soft:CsnStr   = csnstrlower(notes)
    csnprint(soft)
    csnstrupper(soft)
    csnprint(soft)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
CsnStr(shape=(4,), dtype=str)
['c4' 'eb4' 'g#4' 'détaché']
CsnStr(shape=(4,), dtype=str)
['C4' 'EB4' 'G#4' 'DéTACHé']
```

## See also

* [csnstrupper](csnstrupper.md)
* [csnstrcapitalize](csnstrcapitalize.md)
* [csnstrswapcase](csnstrswapcase.md)
* [csnstrtitle](csnstrtitle.md)
* [csnstradd](csnstradd.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
