# csnrtunlock

## Abstract

Clear the real-time allocation guard from an array.

## Description

`csnrtunlock` clears the mark installed by [csnrtlock](csnrtlock.md), or
inherited from an audio or marked source. Once cleared, the specified array may
reallocate during performance if a k-rate operation changes its required
capacity.

The operation affects only `handle`. Arrays already derived from it keep the
mark they inherited; arrays created from it after the unlock inherit the cleared
state. Apply it to the exact branch that is allowed to allocate:

```csound
source:CsnArr = csnfromaudio(signal) ; marked by default
analysis:CsnArr = csncopy(source)    ; inherits the mark
csnrtunlock analysis                 ; only this branch may now resize
```

`csnrtunlock` does not allocate and does not change the data or shape. Its
optional k-rate trigger clears the mark on every non-zero pass and is inert when
zero, including at initialization.

`csnrtlock(handle, 0)` is not an unlock operation: with a zero k-rate trigger it
does nothing. Use `csnrtunlock` explicitly.

## Syntax

```csound
csnrtunlock(handle:CsnArr)
csnrtunlock(handle:CsnArr, trig:k)
```

## Arguments

* `handle:CsnArr`: array whose real-time mark is cleared.
* `trig:k` (optional, default `1`): clear the mark when non-zero; do nothing when zero.

## Output

None.

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
sr = 48000
ksmps = 32
0dbfs = 1

Source@global:CsnArr = csnfromarray(array(0, 10, 20, 30))
Output@global:CsnArr = csnempty(array(0))

instr 1
    csnrtlock Source
    kLength init 4
    if timeinstk() >= 3 then
        kLength = 6
    endif
    Output = csnresample(Source, kLength, 0, 1)
    ; Output inherited the mark, but this analysis branch may resize.
    csnrtunlock Output
    kSize = csnsize(Output)
    if timeinstk() == 5 then
        printf("unlocked output grew to %d elements\n", 1, kSize)
    endif
endin
</CsInstruments>
<CsScore>
i 1 0 0.01
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnrtlock](csnrtlock.md)
* [csnfromaudio](csnfromaudio.md)
* [csnresample](csnresample.md)

## Credits

Pasquale Mainolfi, 2026
