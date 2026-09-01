# csnempty

## Abstract

Reserve a shape without publishing any element.

## Description

`csnempty` allocates room for an array of the requested shape but publishes no
element: [csnsize](csnsize.md) reports `0` and [csnisempty](csnisempty.md)
reports `1`, while [csnshape](csnshape.md) still reports the extents that were
reserved.

That reservation is the capacity [csnpush](csnpush.md) fills, so an array built
up element by element only reallocates when it outgrows what was reserved. It is
also the idiomatic way to declare a `@global` output slot before the note that
will fill it exists.

An empty array is an ordinary value in csnum, not an error case: the shape
transforms return an empty result of the right rank, concatenation with an empty
operand yields the other one, and [csnsum](csnsum.md) over an empty array is `0`.

`itype` is read at init, so an array can be declared empty *and* complex from the
start with `csnempty(shape, 1)`.

## Syntax

```csound
handle:CsnArr = csnempty(shape:i[])
handle:CsnArr = csnempty(shape:i[], itype:i)
handle:CsnArr = csnempty(shape:k[])
handle:CsnArr = csnempty(shape:k[], itype:i)
```

## Arguments

* `shape:i[] / shape:k[]`: the extents to reserve, one per dimension.
* `itype:i` (optional, default `0`): `0` for a real array, `1` for a complex one.

## Output

* `handle:CsnArr`: handle of the new, empty array.

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
; csnempty.csd
;
; csnempty reserves a shape without publishing any element. The reservation is
; the capacity csnpush fills, so pushing up to it never reallocates.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    cap:i[]     = fillarray(4)
    buf:CsnArr  = csnempty(cap)

    size:i      = csnsize(buf)
    empty:i     = csnisempty(buf)
    shape:i[]   = csnshape(buf)
    prints("size = %d, isempty = %d, reserved extent = %g\n", size, empty, shape[0])

    csnpush(buf, 10)
    csnpush(buf, 20)
    filled:i    = csnsize(buf)
    buf_out:i[] = csntoarray(buf)
    prints("after two pushes: size = %d, values = %g %g\n", filled, buf_out[0], buf_out[1])

    ; an empty array travels through the suite instead of stopping it
    fresh:CsnArr = csnempty(cap)
    total:i      = csnsum(fresh)
    prints("sum over an empty array = %g\n", total)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnpush](csnpush.md)
* [csnpop](csnpop.md)
* [csnisempty](csnisempty.md)
* [csnzeros](csnzeros.md)

## Credits

Pasquale Mainolfi, 2026
