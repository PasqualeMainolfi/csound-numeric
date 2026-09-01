# csnload

## Abstract

Read an array back from a `.csn` file.

## Description

`csnload` reads a file written by [csnsave](csnsave.md) and publishes a handle to
the array it holds. The shape and the element type come from the file's header,
so the array is restored rather than guessed at.

Every field is validated on the way in. A truncated file, a shape whose element
count contradicts the declared payload length, an unknown element type, or a
version this build does not know are all rejected with a message naming the
field, rather than producing a plausible-looking array from garbage. The path
must end in `.csn`.

At k-rate the trigger is the whole contract: it fires, the file is read. There is
deliberately no caching between triggers, not even on an unchanged path. `csnload`
reads a file it does not own, so the path proves nothing about the bytes behind
it, and a stat-based stamp would only narrow the window — on HFS+, SMB/NFS and
FAT the mtime granularity is one to two seconds, wide enough for a same-size
rewrite to hide in.

Until the first trigger fires the handle still holds the empty array the init
pass published, so a consumer that cannot read an empty extent belongs behind the
trigger too.

## Syntax

```csound
handle:CsnArr = csnload(path:S)
handle:CsnArr = csnload(path:S, trig:k)
```

## Arguments

* `path:S`: the file to read; must end in `.csn`.
* `trig:k`: k-rate trigger. The file is read on a non-zero trigger and left alone on a zero one.

## Output

* `handle:CsnArr`: handle of the array read from the file. Empty until the first k-rate trigger fires.

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
; csnload.csd
;
; csnload restores shape and element type from the file header. At k-rate the
; trigger is the whole contract, and the handle is empty until it first fires.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]  = fillarray(3, 2)
    src:CsnArr = csnreshape(csnfromarray(array(10, 20, 30, 40, 50, 60)), shape)
    csnsave(src, "csnload_example.csn")
    prints("written\n")
    turnoff
endin

instr 2
    back:CsnArr     = csnload("csnload_example.csn")
    back_shape:i[]  = csnshape(back)
    back_out:i[]    = csntoarray(csnflatten(back))
    prints("shape = %g x %g, values = %g %g %g %g %g %g\n", back_shape[0], back_shape[1], back_out[0], back_out[1], back_out[2], back_out[3], back_out[4], back_out[5])
    turnoff
endin

instr 3
    ; k-rate: empty until the trigger fires
    elapsed:k   = timeinsts()
    trig:k      = (elapsed > 0.02 ? 1 : 0)
    live:CsnArr = csnload("csnload_example.csn", trig)
    n:k         = csnsize(live)
    printf("size after trigger = %d\n", trig, n)
endin

</CsInstruments>
<CsScore>
i 1 0   0.1
i 2 0.2 0.1
i 3 0.4 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsave](csnsave.md)
* [csnisempty](csnisempty.md)
* [csnfromftable](csnfromftable.md)

## Credits

Pasquale Mainolfi, 2026
