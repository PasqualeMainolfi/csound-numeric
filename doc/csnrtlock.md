# csnrtlock

## Abstract

Mark an array as belonging to a real-time path.

## Description

`csnrtlock` marks a handle so that its array cannot be reallocated during
performance. Arrays derived from a marked source inherit the mark when they are
created. If a later k-rate shape or item-type change would require allocation,
the operation raises a performance error naming the output instead of allocating
on the real-time thread.

Audio sources such as [csnfromaudio](csnfromaudio.md), [csnpack](csnpack.md) and
[csnsnap](csnsnap.md) apply this mark by default. `csnrtlock` is intended for
other k-rate chains that also run under a deadline.

The mark travels forward at array creation time. Put `csnrtlock` after the
source and before the opcodes that must inherit it:

```csound
source:CsnArr = csnzeros(shape)
csnrtlock source
derived:CsnArr = csncopy(source) ; inherits the mark
```

Locking `source` later does not retroactively change arrays already derived from
it. Similarly, [csnrtunlock](csnrtunlock.md) clears only the specified handle;
it does not clear existing descendants.

Only reallocation during performance is forbidden. Ordinary value updates and
layout changes that reuse the existing capacity remain allowed. The optional
k-rate trigger applies the lock on every non-zero pass and is inert when zero,
including at initialization.

The same rule covers in-place growth past an array's capacity (`csnpush`,
`csninsert`, `csnsetinsert`, and `csnpad` or `csnresize` without an output) and
the working buffers opcodes keep for themselves. Those buffers are reserved at
init for the most the array they serve can hold, so a marked path does not need
to grow them even when the lock is applied after the opcode. `csnsave`,
`csnload` and `csnprint` do file or console I/O at perf time and stay outside
the guarantee.

### Audio frames are already marked

There is normally no explicit `csnrtlock` call in a `csnsnap` analysis chain:

```csound
frame:CsnArr, kready = csnsnap(asig, 1024, 256)
spectrum:CsnArr = csnrfft(frame, 1024, -1, kready)
```

`csnsnap` uses `irt=1` by default, so `frame` is marked and `spectrum` inherits
the mark when the FFT initializes. Use `csnrtlock` explicitly for sources that
do not come from the audio bridge, as in the complete example below.

## Syntax

```csound
csnrtlock(handle:CsnArr)
csnrtlock(handle:CsnArr, trig:k)
```

## Arguments

* `handle:CsnArr`: array to mark.
* `trig:k` (optional, default `1`): apply the mark when non-zero; do nothing when zero.

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

instr 1
    source:CsnArr = csnfromarray(array(1, 2, 3, 4))
    csnrtlock source

    ; This result inherits the mark. Its fixed shape allocates at init and
    ; therefore needs no allocation on the performance thread.
    kTrig init 1
    spectrum:CsnArr = csnrfft(source, 4, -1, kTrig)
    kSize = csnsize(spectrum)
    if timeinstk() == 2 then
        printf("locked fixed-size spectrum: %d bins\n", 1, kSize)
    endif
endin
</CsInstruments>
<CsScore>
i 1 0 0.01
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnrtunlock](csnrtunlock.md)
* [csnrtlockstart](csnrtlockstart.md)
* [csnrtlockend](csnrtlockend.md)
* [csnrtlockall](csnrtlockall.md)
* [csnfromaudio](csnfromaudio.md)
* [csnpack](csnpack.md)
* [csnsnap](csnsnap.md)

## Credits

Pasquale Mainolfi, 2026
