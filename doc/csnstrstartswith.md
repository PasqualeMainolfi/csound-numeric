# csnstrstartswith

## Abstract

Elementwise prefix test, as `np.strings.startswith`.

## Description

`csnstrstartswith` returns, for each string, `1` when it begins with `prefix`
and `0` otherwise, in a real array with the shape of the source: a mask for
[csnwhere](csnwhere.md), [csnselect](csnselect.md) or
[csncompress](csncompress.md). Every string begins with the empty prefix. The
test compares bytes, as NumPy does on a bytes (`S`) array.

The test looks at the bytes from `start` up to `end`, `end` excluded, as
`s[start:end]` in Python: `end` is clamped to each string and `-1`, its
default, stands for the end of the string, and a `prefix` longer than that
range never matches. As in [csnstrsubstr](csnstrsubstr.md), `start` and `end`
are whole numbers from 0 up, `start` no greater than `end`; NumPy's negative
indices are refused. So `csnstrstartswith(names, "cd", 2)` tests the bytes
from 2 on.

With a k `start` or `end` the k form runs, and runs again when the source, the
string or the range changed. Its trigger is optional (default `1`) but comes
after the range, so a call with a trigger names the range too:
`csnstrstartswith(names, Sx, 0, -1, kTrig)`.

The output is a real array in the shape of the source, so the k form writes
its numbers into the elements the init pass laid out and allocates nothing
while the source keeps its shape: it may run on a [real-time
path](csnrtlock.md).

On one `S` it returns a number: `csnstrstartswith("kick.wav", "kick")` gives
1, with the same range rules, an `end` past the string reading to its end. The
`i` output takes the init-time form; a `k` output takes the k form, whose
`start` and `end` are k values: `csnstrstartswith(Sname, "x", 0, -1, kTrig)`.
An `S` has no rate, so the k form takes a trigger, and computes at init too,
when its range is known there.

## Syntax

```csound
out:CsnArr = csnstrstartswith(source:CsnStr, prefix:S [, start:i [, end:i]])
out:CsnArr = csnstrstartswith(source:CsnStr, prefix:S, start:k [, end:k [, trig:k]])
out:i = csnstrstartswith(source:S, prefix:S [, start:i [, end:i]])
out:k = csnstrstartswith(source:S, prefix:S [, start:k [, end:k [, trig:k]]])
```

## Arguments

* `source:CsnStr`: the strings to query; or `source:S`, one string.
* `prefix:S`: the bytes each string should begin with.
* `start:i / start:k`: the first byte looked at, from 0 (default `0`).
* `end:i / end:k`: the byte the search stops before, clamped to each string; `-1` (default) for the end of the string; any other end no smaller than `start`.
* `trig:k`: k-rate trigger, optional (default `1`). A zero trigger republishes the previous result.

## Output

* `out:CsnArr`: a 0/1 mask in the shape of `source`.
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
; csnstrstartswith.csd
;
; Which samples are snares, then only those.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    files:S[]     = fillarray("snare_01.wav", "kick_01.wav", "snare_02.wav")
    raw:CsnStr    = csnfromarray(files)
    snare:CsnArr  = csnstrstartswith(raw, "snare")
    csnprint(snare)
    only:CsnStr   = csncompress(raw, snare)
    csnprint(only)
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
[1 0 1]
CsnStr(shape=(2,), dtype=str)
['snare_01.wav' 'snare_02.wav']
```

## See also

* [csnstrendswith](csnstrendswith.md)
* [csnstrfind](csnstrfind.md)
* [csncompress](csncompress.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
