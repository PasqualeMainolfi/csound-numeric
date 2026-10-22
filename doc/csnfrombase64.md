# csnfrombase64

## Abstract

base64 text to bytes, as Python's `base64.b64decode`.

## Description

`csnfrombase64` decodes base64 text, the standard alphabet of RFC 4648, into
the bytes it holds: `csnfrombase64("TWFu")` gives `"Man"`. An `S` output
takes the bytes as a string; a `:CsnArr;` output takes them as a real
one-dimensional array, each from 0 to 255, and `""` gives an empty one. A
string cannot hold a zero byte, so text that decodes to one is refused for an
`S` output, the message naming its index; the array holds it.

The text must be canonical, as [csntobase64](csntobase64.md) writes it: a
length that is a multiple of 4, padded with `=` only at the end, no blank or
line break, and the unused bits of the last group zero. Anything else is
refused, the message naming the group of 4 where it goes wrong. Python's
`b64decode` is laxer on the last rule (`"TWF="` gives it `b'Ma'`, here it is
refused), and the URL-safe alphabet (`-`, `_`) is not read.

A string has no rate, so the k forms take a trigger, and compute at init too.
The text has no version to compare, so every pass that runs decodes it again;
the array follows the length of the bytes, and on a [real-time
path](csnrtlock.md) a pass that needs more room than the init pass gave it
stops with a performance error. Like `strcpyk`, an `S` output takes the room
it needs at perf.

## Syntax

```csound
out:S = csnfrombase64(source:S)
out:S = csnfrombase64(source:S, trig:k)
out:CsnArr = csnfrombase64(source:S)
out:CsnArr = csnfrombase64(source:S, trig:k)
```

## Arguments

* `source:S`: base64 text, padded.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger keeps the previous result.

## Output

* `out:S`: the bytes as a string, none of them 0.
* `out:CsnArr`: the bytes, a real 1-D array.

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
; csnfrombase64.csd
;
; base64 text back to a string, and to the bytes of a MIDI identity
; request.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    name:S         = csnfrombase64("YmFzcyBwYWQ=")
    prints("name %s\n", name)
    sysex:CsnArr   = csnfrombase64("8H5/BgH3")
    csnprint(sysex)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
name bass pad
CsnArr(shape=(6,), dtype=float64)
[240 126 127 6 1 247]
```

## See also

* [csntobase64](csntobase64.md)
* [csnstrfromascii](csnstrfromascii.md)
* [csnfromhex](csnfromhex.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
