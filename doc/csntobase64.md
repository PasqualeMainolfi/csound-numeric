# csntobase64

## Abstract

Bytes to base64 text, as Python's `base64.b64encode`.

## Description

`csntobase64` writes bytes as base64 text, the standard alphabet of RFC 4648
(`A`-`Z`, `a`-`z`, `0`-`9`, `+`, `/`) padded with `=` to a multiple of 4
characters, with no line breaks: `csntobase64("Man")` gives `"TWFu"`,
`csntobase64("Ma")` gives `"TWE="`, and an empty source gives `""`. The text
holds only ASCII, so it travels where bytes cannot, in an OSC string, a JSON
value or a file name.

The bytes come from one `S`, every byte of it, or from a real
one-dimensional `:CsnArr;` of whole numbers from 0 to 255, which may hold
the 0 that a string cannot. A value out of that range, a complex array or
one of more dimensions is refused, the message naming the first bad value and
its index. [csnfrombase64](csnfrombase64.md) decodes the text back.

Both k forms take a trigger, and compute at init too. The array form writes
the text again only when the array changed; an `S` has no version to compare,
so the string form encodes on every pass that runs. Like `strcpyk`, the output
takes the room it needs at perf.

## Syntax

```csound
out:S = csntobase64(source:S)
out:S = csntobase64(source:S, trig:k)
out:S = csntobase64(source:CsnArr)
out:S = csntobase64(source:CsnArr, trig:k)
```

## Arguments

* `source:S`: the bytes of a string.
* `source:CsnArr`: a real 1-D array of bytes, whole numbers from 0 to 255.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger keeps the previous text.

## Output

* `out:S`: the base64 text, padded.

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
; csntobase64.csd
;
; A patch name and a MIDI identity request, as base64 text that travels in
; JSON or an OSC string.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    name:S         = csntobase64("bass pad")
    prints("name %s\n", name)
    request:i[]    = fillarray(240, 126, 127, 6, 1, 247)
    sysex:CsnArr   = csnfromarray(request)
    text:S         = csntobase64(sysex)
    prints("sysex %s\n", text)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
name YmFzcyBwYWQ=
sysex 8H5/BgH3
```

## See also

* [csnfrombase64](csnfrombase64.md)
* [csnstrtoascii](csnstrtoascii.md)
* [csntohex](csntohex.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
