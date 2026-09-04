# csnshuffle

## Abstract

Randomly permute an array in place.

## Description

`csnshuffle` rearranges the elements of a real array in place with a uniform
Fisher-Yates shuffle. The shape is unchanged. For a multidimensional array, the
flat element order is shuffled rather than one axis at a time.

The generator is owned by the Csound instance and shared with [csnrand](csnrand.md)
and [csnrandint](csnrandint.md). Use [csnseed](csnseed.md) to make the permutation
reproducible.

At k-rate, the permutation happens only when the trigger is non-zero. A zero
trigger leaves the array unchanged. The trigger must be present because it is
the argument that selects the performance-time overload.

Real only.

## Syntax

```csound
csnshuffle(handle:CsnArr)
csnshuffle(handle:CsnArr, trig:k)
```

## Arguments

* `handle:CsnArr`: real array to shuffle in place.
* `trig:k`: required k-rate trigger. A non-zero value shuffles the array; zero does nothing.

## Output

None. The source array is modified in place.

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
; csnshuffle.csd
;
; csnshuffle randomly permutes an array in place. Its shape stays unchanged;
; for a multidimensional array, the operation shuffles the flat element order.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    values:CsnArr = csnfromarray(array(1, 2, 3, 4, 5, 6))

    prints("before:\n")
    csnprint(values)

    csnseed(12345)
    csnshuffle(values)

    prints("after:\n")
    csnprint(values)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnrand](csnrand.md)
* [csnrandint](csnrandint.md)
* [csnseed](csnseed.md)
* [csnsort](csnsort.md)

## Credits

Pasquale Mainolfi, 2026
