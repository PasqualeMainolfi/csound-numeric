# csnstrswapcase

## Abstract

Elementwise swap of upper and lower case in strings, as `np.strings.swapcase`.

## Description

`csnstrswapcase` turns every letter of each string into the other case:
capitals go down and lower-case letters go up, so `"Cmaj7"` becomes
`"cMAJ7"`. Everything that is not a letter, digits, spaces and punctuation,
stays as it is.

Only the ASCII letters change: every byte from 0x80 up is left as it is,
whatever the locale, so a UTF-8 string keeps its multibyte characters
(`"Dé"` gives `"dé"`). This is what `np.strings.swapcase` does on a bytes
(`S`) array; on a `str` array NumPy swaps the other Unicode letters too.

Two forms share the name. The one with an output writes a new `:CsnStr;`
holding its own strings and leaves the source alone. The one without an
output rewrites the source in place and returns nothing.

Swapping twice gives the strings back, so unlike the other case conversions
the k form cannot skip a pass on its own result. Its output may be its source,
`names = csnstrswapcase(names, ktrig)`, and then every pass that runs swaps
again, as `X = csnmul(X, 0.99)` scales again; the init pass leaves that case
to the first k pass, so the first cycle swaps once. Otherwise the k forms run
again only when the source changed, and the in-place k form swaps once for
each change made by someone else. The k forms take an explicit trigger, and
the call without one is the init-time form.

A string keeps its length, so the in-place form writes into the buffers its
elements already hold and allocates nothing, and may run on a [real-time
path](csnrtlock.md). The form with an output writes each string into the room
its element already holds: on a real-time path the init pass gives a marked
output room for the source's longest string, and the k form stops with a
performance error at the first string that does not fit. See [string
arrays](string-arrays.md#real-time-paths).

## Syntax

```csound
out:CsnStr = csnstrswapcase(source:CsnStr)
out:CsnStr = csnstrswapcase(source:CsnStr, trig:k)
csnstrswapcase(source:CsnStr)
csnstrswapcase(source:CsnStr, trig:k)
```

## Arguments

* `source:CsnStr`: the strings whose case is swapped.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger republishes the previous result.

## Output

* `out:CsnStr`: the swapped strings, in the shape of `source`. Omit it for the in-place form.

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
; csnstrswapcase.csd
;
; Chord symbols with their case swapped as a copy, then swapped back in place.
; Only letters change: "#", "7" and the space stay where they are.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    names:S[]      = fillarray("Cmaj7", "f#m", "Bb dim")
    chords:CsnStr  = csnfromarray(names)
    swapped:CsnStr = csnstrswapcase(chords)
    csnprint(swapped)
    csnstrswapcase(swapped)
    csnprint(swapped)
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
['cMAJ7' 'F#M' 'bB DIM']
CsnStr(shape=(3,), dtype=str)
['Cmaj7' 'f#m' 'Bb dim']
```

## See also

* [csnstrupper](csnstrupper.md)
* [csnstrlower](csnstrlower.md)
* [csnstrcapitalize](csnstrcapitalize.md)
* [csnstrtitle](csnstrtitle.md)
* [csnstradd](csnstradd.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
