# csnstrendswith

## Abstract

Elementwise suffix test, as `np.strings.endswith`.

## Description

`csnstrendswith` returns, for each string, `1` when it ends with `suffix` and
`0` otherwise, in a real array with the shape of the source: a mask for
[csnwhere](csnwhere.md), [csnselect](csnselect.md) or
[csncompress](csncompress.md). Every string ends with the empty suffix. The
test compares bytes, as NumPy does on a bytes (`S`) array.

The test looks at the bytes from `start` up to `end`, `end` excluded, as
`s[start:end]` in Python: `end` is clamped to each string and `-1`, its
default, stands for the end of the string, and a `suffix` longer than that
range never matches. As in [csnstrsubstr](csnstrsubstr.md), `start` and `end`
are whole numbers from 0 up, `start` no greater than `end`; NumPy's negative
indices are refused. So `csnstrendswith(names, ".wav", 0, 8)` tests whether
the first 8 bytes end with `.wav`.

With a k `start` or `end` the k form runs, and runs again when the source, the
string or the range changed. Its trigger is optional (default `1`) but comes
after the range, so a call with a trigger names the range too:
`csnstrendswith(names, Sx, 0, -1, kTrig)`.

The output is a real array in the shape of the source, so the k form writes
its numbers into the elements the init pass laid out and allocates nothing
while the source keeps its shape: it may run on a [real-time
path](csnrtlock.md).

## Syntax

```csound
out:CsnArr = csnstrendswith(source:CsnStr, suffix:S [, start:i [, end:i]])
out:CsnArr = csnstrendswith(source:CsnStr, suffix:S, start:k [, end:k [, trig:k]])
```

## Arguments

* `source:CsnStr`: the strings to query.
* `suffix:S`: the bytes each string should end with.
* `start:i / start:k`: the first byte looked at, from 0 (default `0`).
* `end:i / end:k`: the byte the search stops before, clamped to each string; `-1` (default) for the end of the string; any other end no smaller than `start`.
* `trig:k`: k-rate trigger, optional (default `1`). A zero trigger republishes the previous result.

## Output

* `out:CsnArr`: a 0/1 mask in the shape of `source`.

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
; csnstrendswith.csd
;
; Which files are WAV files, then which were before a ".bak" was added:
; the first 9 bytes end with ".wav".
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    files:S[]    = fillarray("take1.wav", "take2.aif", "take3.wav.bak")
    raw:CsnStr   = csnfromarray(files)
    wav:CsnArr   = csnstrendswith(raw, ".wav")
    csnprint(wav)
    was:CsnArr   = csnstrendswith(raw, ".wav", 0, 9)
    csnprint(was)
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
[1 0 0]
CsnArr(shape=(3,), dtype=float64)
[1 0 1]
```

## See also

* [csnstrstartswith](csnstrstartswith.md)
* [csnstrrfind](csnstrrfind.md)
* [csnwhere](csnwhere.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
