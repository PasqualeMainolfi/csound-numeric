# csnstrrfind

## Abstract

Elementwise highest index of a substring, as `np.strings.rfind`.

## Description

`csnstrrfind` returns, for each string, the index of the first byte of the
last occurrence of `sub`, or `-1` when the string does not contain it, in a
real array with the shape of the source. Overlapping occurrences count, so
`"aaa"` finds `"aa"` at `1`, and an empty `sub` is found at the end, at the
length of the string. The index counts bytes, as NumPy does on a bytes (`S`)
array.

The search looks at the bytes from `start` up to `end`, `end` excluded, as
`s[start:end]` in Python: `end` is clamped to each string and `-1`, its
default, stands for the end of the string, and the index returned still counts
from the start of the whole string. A `start` past the end of a string finds
nothing there, not even an empty `sub`. As in [csnstrsubstr](csnstrsubstr.md),
`start` and `end` are whole numbers from 0 up, `start` no greater than `end`;
NumPy's negative indices are refused.

With a k `start` or `end` the k form runs, and runs again when the source, the
string or the range changed. Its trigger is optional (default `1`) but comes
after the range, so a call with a trigger names the range too:
`csnstrrfind(names, Sx, 0, -1, kTrig)`.

The output is a real array in the shape of the source, so the k form writes
its numbers into the elements the init pass laid out and allocates nothing
while the source keeps its shape: it may run on a [real-time
path](csnrtlock.md).

## Syntax

```csound
out:CsnArr = csnstrrfind(source:CsnStr, sub:S [, start:i [, end:i]])
out:CsnArr = csnstrrfind(source:CsnStr, sub:S, start:k [, end:k [, trig:k]])
```

## Arguments

* `source:CsnStr`: the strings to query.
* `sub:S`: the substring to look for.
* `start:i / start:k`: the first byte looked at, from 0 (default `0`).
* `end:i / end:k`: the byte the search stops before, clamped to each string; `-1` (default) for the end of the string; any other end no smaller than `start`.
* `trig:k`: k-rate trigger, optional (default `1`). A zero trigger republishes the previous result.

## Output

* `out:CsnArr`: the indices, `-1` where `sub` is missing, in the shape of `source`.

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
; csnstrrfind.csd
;
; Where the extension starts in each file name: the last ".".
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    files:S[]   = fillarray("take.1.wav", "loop.aif", "notes")
    raw:CsnStr  = csnfromarray(files)
    dot:CsnArr  = csnstrrfind(raw, ".")
    csnprint(dot)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
CsnArr(shape=(3,), dtype=float64)
[6 4 -1]
```

## See also

* [csnstrfind](csnstrfind.md)
* [csnstrendswith](csnstrendswith.md)
* [csnstrsubstr](csnstrsubstr.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
