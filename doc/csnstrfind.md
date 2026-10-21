# csnstrfind

## Abstract

Elementwise lowest index of a substring, as `np.strings.find`.

## Description

`csnstrfind` returns, for each string, the index of the first byte of the
first occurrence of `sub`, or `-1` when the string does not contain it, in a
real array with the shape of the source. An empty `sub` is found at `0`. The
index counts bytes, as NumPy does on a bytes (`S`) array.

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
`csnstrfind(names, Sx, 0, -1, kTrig)`.

The output is a real array in the shape of the source, so the k form writes
its numbers into the elements the init pass laid out and allocates nothing
while the source keeps its shape: it may run on a [real-time
path](csnrtlock.md).

On one `S` it returns a number: `csnstrfind("abcabc", "c", 3)` gives 5, with
the same range rules, an `end` past the string reading to its end. The `i`
output takes the init-time form; a `k` output takes the k form, whose `start`
and `end` are k values: `csnstrfind(Sname, "x", 0, -1, kTrig)`. An `S` has no
rate, so the k form takes a trigger, and computes at init too, when its range
is known there.

## Syntax

```csound
out:CsnArr = csnstrfind(source:CsnStr, sub:S [, start:i [, end:i]])
out:CsnArr = csnstrfind(source:CsnStr, sub:S, start:k [, end:k [, trig:k]])
out:i = csnstrfind(source:S, sub:S [, start:i [, end:i]])
out:k = csnstrfind(source:S, sub:S [, start:k [, end:k [, trig:k]]])
```

## Arguments

* `source:CsnStr`: the strings to query; or `source:S`, one string.
* `sub:S`: the substring to look for.
* `start:i / start:k`: the first byte looked at, from 0 (default `0`).
* `end:i / end:k`: the byte the search stops before, clamped to each string; `-1` (default) for the end of the string; any other end no smaller than `start`.
* `trig:k`: k-rate trigger, optional (default `1`). A zero trigger republishes the previous result.

## Output

* `out:CsnArr`: the indices, `-1` where `sub` is missing, in the shape of `source`.
* `out:i`, `out:k`: the result for one string, for an `S` source.

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
; csnstrfind.csd
;
; Where the take number starts in each sample name: the first "_".
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    files:S[]     = fillarray("kick_01.wav", "snare_02.wav", "hat.wav")
    raw:CsnStr    = csnfromarray(files)
    under:CsnArr  = csnstrfind(raw, "_")
    csnprint(under)
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
[4 5 -1]
```

## See also

* [csnstrrfind](csnstrrfind.md)
* [csnstrstartswith](csnstrstartswith.md)
* [csnstrsubstr](csnstrsubstr.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
