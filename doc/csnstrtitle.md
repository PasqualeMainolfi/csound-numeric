# csnstrtitle

## Abstract

Elementwise title case of strings, as `np.strings.title`.

## Description

`csnstrtitle` puts each string in title case, as Python's `str.title`: a
letter that follows anything other than a letter goes up, and a letter that
follows a letter goes down, so `"BASS-DROP"` becomes `"Bass-Drop"`. Digits
are not letters, so a letter after one goes up too: `"1st take"` becomes
`"1St Take"` and `"x2y"` becomes `"X2Y"`, as in Python and NumPy.

Only the ASCII letters change, whatever the locale, and only they count as
letters. This is what `np.strings.title` does on a bytes (`S`) array: the
bytes of a UTF-8 character such as `é` stay as they are and count as a
non-letter, so the letter after one goes up, `"Cafés"` becoming `"CaféS"`
where NumPy on a `str` array gives `"Cafés"`. Title case is for ASCII names;
text with accented letters is better left as it is.

Two forms share the name. The one with an output writes a new `:CsnStr;`
holding its own strings and leaves the source alone; the output may also be
the source, `names = csnstrtitle(names)`. The one without an output rewrites
the source in place and returns nothing.

The k forms take an explicit trigger, and the call without one is the
init-time form. They run again only when the source changed: title case
applied twice gives what it gave once.

A string keeps its length, so the in-place form writes into the buffers its
elements already hold and allocates nothing, and may run on a [real-time
path](csnrtlock.md). The form with an output writes each string into the room
its element already holds: on a real-time path the init pass gives a marked
output room for the source's longest string, and the k form stops with a
performance error at the first string that does not fit. See [string
arrays](string-arrays.md#real-time-paths).

On one `S` it returns an `S`: `csnstrtitle("bass-drop")` gives "Bass-Drop". It
has no in-place form, as a string argument is an input: `Sx = csnstrtitle(Sx)`
converts `Sx`. An `S` has no rate, so the k form takes a trigger, and computes
at init too. Like `strcpyk`, its output takes the room it needs at perf.

## Syntax

```csound
out:CsnStr = csnstrtitle(source:CsnStr)
out:CsnStr = csnstrtitle(source:CsnStr, trig:k)
csnstrtitle(source:CsnStr)
csnstrtitle(source:CsnStr, trig:k)
out:S = csnstrtitle(source:S)
out:S = csnstrtitle(source:S, trig:k)
```

## Arguments

* `source:CsnStr`: the strings to put in title case; or `source:S`, one string.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger republishes the previous result.

## Output

* `out:CsnStr`: the strings in title case, in the shape of `source`. Omit it for the in-place form.
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
; csnstrtitle.csd
;
; Cue names in title case as a copy, then in place. Every letter after a
; non-letter goes up, so the "t" after "2" in "take 2take" does too.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    names:S[]    = fillarray("intro pad", "BASS-DROP", "take 2take")
    cues:CsnStr  = csnfromarray(names)
    shown:CsnStr = csnstrtitle(cues)
    csnprint(shown)
    csnstrtitle(cues)
    csnprint(cues)
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
['Intro Pad' 'Bass-Drop' 'Take 2Take']
CsnStr(shape=(3,), dtype=str)
['Intro Pad' 'Bass-Drop' 'Take 2Take']
```

## See also

* [csnstrupper](csnstrupper.md)
* [csnstrlower](csnstrlower.md)
* [csnstrcapitalize](csnstrcapitalize.md)
* [csnstrswapcase](csnstrswapcase.md)
* [csnstradd](csnstradd.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
