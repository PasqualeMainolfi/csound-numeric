# csnstrsubstr

## Abstract

Elementwise substrings, as `np.strings.slice`.

## Description

`csnstrsubstr` keeps, from each string, the bytes from `start` up to `stop`,
`stop` excluded, taking every `step`-th: `"abcdef"` from 1 to 4 becomes
`"bcd"`, and from 0 to 100 with step 2 `"ace"`. The range is clamped to each
string, so a `stop` past the end reads to the end and a `start` past it gives
`""`: one range fits strings of any length, and a large `stop` stands for
"to the end".

The indices follow [csngetslice](csngetslice.md): whole numbers from 0 up,
`start` no greater than `stop`, and a `step` of at least 1 (default `1`);
anything else is an error. `np.strings.slice` also counts negative indices
from the end, takes a negative step and gives `""` for a `start` past `stop`;
here those are refused. The indices count bytes, as NumPy does on a bytes
(`S`) array, so a range can cut a multibyte UTF-8 character in two.

Two forms share the name. The one with an output writes a new `:CsnStr;`
holding its own strings and leaves the source alone. The one without an
output cuts the source in place and returns nothing.

With k indices the k form runs, and runs again when the source or the range
changed. Its trigger is optional (default `1`) but comes after the step, so a
call with a trigger names the step too: `csnstrsubstr(names, kStart, kStop, 1,
kTrig)`. A substring cut again is shorter again, so an output that is its own
source, `names = csnstrsubstr(names, 1, 100, 1, kTrig)`, cuts again on every
pass that runs, and the in-place k form once for each change.

A substring is never longer than its string, so an output holds the room its
source's longest string gave it at init and the in-place form writes inside
the buffers its elements hold: both may run on a [real-time
path](csnrtlock.md). In place, a cut cannot be undone, so the init pass cuts
only with a range it knows, constants or i values, and leaves a k range to
the first pass. See [string arrays](string-arrays.md#real-time-paths).

On one `S` it returns an `S`, cut by the same rules: `csnstrsubstr("abcdef",
1, 4)` gives `"bcd"`. It has no in-place form, as a string argument is an
input: `Sx = csnstrsubstr(Sx, 1, 4)` cuts `Sx`. An `S` has no rate, so the k
form takes a trigger, and computes at init too, when its range is known there.
Like `strcpyk`, its output takes the room it needs at perf.

## Syntax

```csound
out:CsnStr = csnstrsubstr(source:CsnStr, start:i, stop:i [, step:i])
out:CsnStr = csnstrsubstr(source:CsnStr, start:k, stop:k [, step:k [, trig:k]])
csnstrsubstr(source:CsnStr, start:i, stop:i [, step:i])
csnstrsubstr(source:CsnStr, start:k, stop:k [, step:k [, trig:k]])
out:S = csnstrsubstr(source:S, start:i, stop:i [, step:i])
out:S = csnstrsubstr(source:S, start:k, stop:k [, step:k [, trig:k]])
```

## Arguments

* `source:CsnStr`: the strings to cut; or `source:S`, one string.
* `start:i / start:k`: the first byte kept, from 0.
* `stop:i / stop:k`: the byte the substring stops before; no smaller than `start`.
* `step:i / step:k`: keep every `step`-th byte, at least 1 (default `1`).
* `trig:k`: k-rate trigger, optional (default `1`). A zero trigger republishes the previous result.

## Output

* `out:CsnStr`: the substrings, in the shape of `source`. Omit it for the in-place form.
* `out:S`: the substring, for an `S` source.

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
; csnstrsubstr.csd
;
; Sample file names reduced to a 4-letter tag as a copy, then to every other
; letter in place.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    files:S[]   = fillarray("kick_01.wav", "snare_02.wav", "hat.wav")
    raw:CsnStr  = csnfromarray(files)
    tags:CsnStr = csnstrsubstr(raw, 0, 4)
    csnprint(tags)
    csnstrsubstr(raw, 0, 100, 2)
    csnprint(raw)
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
['kick' 'snar' 'hat.']
CsnStr(shape=(3,), dtype=str)
['kc_1wv' 'sae0.a' 'htwv']
```

## See also

* [csngetslice](csngetslice.md)
* [csnstrreplace](csnstrreplace.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
