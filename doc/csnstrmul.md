# csnstrmul

## Abstract

Elementwise repetition of strings, as `np.strings.multiply`, or of one string
as Python's `s * n`.

## Description

`csnstrmul` repeats each string of a `:CsnStr;` a number of times. The count
is either one number for every element or a real `:CsnArr;` of counts,
broadcast against the strings NumPy-style. A count of 0 or below gives the
empty string, as NumPy does. A count must be a whole number: `1.5`, an
infinity or a NaN is refused with a message, and so is a complex array of
counts.

The result is a `:CsnStr;` holding its own strings. It may be the string
operand, `pattern = csnstrmul(pattern, 2)`: each element is read before it is
written, and the k form then repeats again on every pass that runs.

The count form with a k count runs again when the count or the strings
change; the array form takes a trigger and runs again when an operand
changed.

Each element is written into the room it already holds. On a [real-time
path](csnrtlock.md) the init pass sizes the elements, a k count at its init
value, so the k form runs as long as every result fits that room: a count no
larger than the one at init, on strings no longer. It checks every element
first and stops with a performance error, before anything is written, when one
does not fit. See [string arrays](string-arrays.md#real-time-paths).

On one `S` it returns an `S`: `csnstrmul("ab", 3)` gives `"ababab"`, with the
same rules for the count, 0 or below giving `""`. A count whose result would
not fit a string size is refused before anything is allocated; one that fits
but is larger than the memory available stops Csound, as for the array form.
With a k count it runs on every pass, its trigger optional (default `1`), and
computes at init with the count's init value; like `strcpyk`, its output
takes the room it needs at perf, also on a real-time path.

The operators `+`, `+=`, `*` and `*=` also work on `S`: `S1 + S2`, `Sx +=
Sy`, `S * n` and `Sx *= n` compute what the named forms compute. Csound has
no arithmetic on strings, so these operators exist only while csnum is
loaded; they add to the language rather than change it, and an orchestra
that uses them needs csnum. Like the other operators they take no trigger and
run on every pass, as `strcatk` does; `Sx += Sx` doubles `Sx`.

## Syntax

```csound
out:CsnStr = csnstrmul(a:CsnStr, counts:CsnArr)
out:CsnStr = csnstrmul(a:CsnStr, counts:CsnArr, trig:k)
out:CsnStr = csnstrmul(a:CsnStr, count:i)
out:CsnStr = csnstrmul(a:CsnStr, count:k)
out:CsnStr = csnstrmul(a:CsnStr, count:k, trig:k)
out:S = csnstrmul(a:S, count:i)
out:S = csnstrmul(a:S, count:k [, trig:k])
out:S = a:S * count
a:S *= count
```

## Arguments

* `a:CsnStr`: the strings repeated; or `a:S`, one string.
* `counts:CsnArr`: real whole numbers, one per element after broadcasting against `a`.
* `count:i / count:k`: one whole number for every element.
* `trig:k`: k-rate trigger; required in the array form, where it selects the k form, and optional (default `1`) with a k count. A zero trigger republishes the previous result.

## Output

* `out:CsnStr`: the repeated strings, in the broadcast shape.
* `out:S`: the repeated string, for an `S` source.

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
; csnstrmul.csd
;
; A text step sequencer: each step is a symbol repeated as many times as its
; count says, and a count of 0 or below leaves the step empty.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    marks:S[]      = fillarray("x", "-", "x", "o")
    steps:CsnStr   = csnfromarray(marks)
    counts:CsnArr  = csnfromarray(array(3, 1, 0, 2))
    pattern:CsnStr = csnstrmul(steps, counts)
    csnprint(pattern)

    twice:CsnStr   = csnstrmul(steps, 2)
    csnprint(twice)

    bar:S          = "x-" * 4
    prints("%s\n", bar)
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
['xxx' '-' '' 'oo']
CsnStr(shape=(4,), dtype=str)
['xx' '--' 'xx' 'oo']
x-x-x-x-
```

## See also

* [csnstradd](csnstradd.md)
* [csnmul](csnmul.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
