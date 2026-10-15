# csnstrcapitalize

## Abstract

Elementwise capitalization of strings, as `np.strings.capitalize`.

## Description

`csnstrcapitalize` turns the first character of each string into its capital
and every other one into its lower-case letter, as Python's `str.capitalize`:
`"cHORUS"` becomes `"Chorus"`. A string that opens with a digit, a space or a
punctuation mark therefore comes out all lower case, `"2nd VERSE"` becoming
`"2nd verse"`, and only the first character is looked at, not the first
letter.

Only the ASCII letters change: every byte from 0x80 up is left as it is,
whatever the locale, so a UTF-8 string keeps its multibyte characters. This is
what `np.strings.capitalize` does on a bytes (`S`) array: a string that opens
with a non-ASCII character keeps it and has the rest lowered (`"éA"` gives
`"éa"`), where NumPy on a `str` array also raises the `é`.

Two forms share the name. The one with an output writes a new `:CsnStr;`
holding its own strings and leaves the source alone; the output may also be
the source, `names = csnstrcapitalize(names)`. The one without an output
rewrites the source in place and returns nothing.

The k forms take an explicit trigger, and the call without one is the
init-time form. They run again only when the source changed: capitalizing
twice gives what capitalizing once gave.

A string keeps its length, so the in-place form writes into the buffers its
elements already hold and allocates nothing, and may run on a [real-time
path](csnrtlock.md). The form with an output writes each string into the room
its element already holds: on a real-time path the init pass gives a marked
output room for the source's longest string, and the k form stops with a
performance error at the first string that does not fit. See [string
arrays](string-arrays.md#real-time-paths).

## Syntax

```csound
out:CsnStr = csnstrcapitalize(source:CsnStr)
out:CsnStr = csnstrcapitalize(source:CsnStr, trig:k)
csnstrcapitalize(source:CsnStr)
csnstrcapitalize(source:CsnStr, trig:k)
```

## Arguments

* `source:CsnStr`: the strings to capitalize.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger republishes the previous result.

## Output

* `out:CsnStr`: the capitalized strings, in the shape of `source`. Omit it for the in-place form.

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
; csnstrcapitalize.csd
;
; Section names typed in any case, capitalized as a copy and then in place.
; The first letter goes up and every other one down, so "2nd VERSE", which
; opens with a digit, comes out all lower case.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    names:S[]       = fillarray("intro", "VERSE", "cHORUS", "2nd VERSE")
    sections:CsnStr = csnfromarray(names)
    titles:CsnStr   = csnstrcapitalize(sections)
    csnprint(titles)
    csnstrcapitalize(sections)
    csnprint(sections)
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
['Intro' 'Verse' 'Chorus' '2nd verse']
CsnStr(shape=(4,), dtype=str)
['Intro' 'Verse' 'Chorus' '2nd verse']
```

## See also

* [csnstrupper](csnstrupper.md)
* [csnstrlower](csnstrlower.md)
* [csnstrswapcase](csnstrswapcase.md)
* [csnstradd](csnstradd.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
