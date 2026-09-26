# csnzlfilter

## Abstract

Filters with the transfer function b / a from a given state, and returns the final state.

## Description

`csnzlfilter` is [csnlfilter](csnlfilter.md) with the filter state in the open, as
`scipy.signal.lfilter(b, a, x, axis, zi) -> (y, zf)`: the filter starts from the state `zi` instead of from rest,
and returns its state after the last sample as `zf`.

`zi` has the layout scipy gives it: the shape of the source with the filtered
axis replaced by the order, `max(len(b), len(a)) - 1`. For a 1-D source that
is a vector of `order` values; for a `(3, 160)` source filtered along axis 1
with order 3, a `(3, 3)` array, one row of states per signal.

Nothing is kept between calls. Every call, and every pass of the k-rate and
audio forms, starts from the `zi` it is given and returns the final state as
`zf`; feeding `zf` back in as the next `zi` filters successive blocks as one
continuous signal. `zi` may be the opcode's own `zf` output, which is the
usual way to write that feedback: it is read before either output is
written.

At k-rate a pass whose trigger is 0, or in which neither the source nor `zi`
has changed, publishes the previous results. At audio rate `zi` is read at
the start of every control period and `zf` rewritten at its end, in place.

The coefficients are copied at init and may not change afterwards. The
output and `zf` are complex when the source, `zi` or any coefficient is; the
audio form takes real coefficients and a real `zi`.

## Syntax

```csound
y:CsnArr, zf:CsnArr csnzlfilter b:CsnArr, a:CsnArr, source:CsnArr, zi:CsnArr
y:CsnArr, zf:CsnArr csnzlfilter b:CsnArr, a:CsnArr, source:CsnArr, zi:CsnArr, axis:i
y:CsnArr, zf:CsnArr csnzlfilter b:CsnArr, a:CsnArr, source:CsnArr, zi:CsnArr, trig:k
y:CsnArr, zf:CsnArr csnzlfilter b:CsnArr, a:CsnArr, source:CsnArr, zi:CsnArr, trig:k, axis:i
aout, zf:CsnArr csnzlfilter b:CsnArr, a:CsnArr, ain, zi:CsnArr
```

## Arguments

* `b:CsnArr`, `a:CsnArr`: 1-D real or complex coefficients, `a[0]` non-zero; both are divided by it.
* `source:CsnArr`: the array to filter, real or complex, of any rank.
* `zi:CsnArr`: the initial state, in the layout above.
* `axis:i` (optional): the axis to filter along. Omit it for the last axis; negative values count from the end.
* `trig:k`: k-rate trigger; a zero trigger republishes the previous results.
* `ain`: the audio signal to filter.

## Output

* `y:CsnArr` / `aout`: the filtered array or signal.
* `zf:CsnArr`: the final state, in the layout of `zi`.

## Execution Time

* Init
* Performance (k-rate)
* Performance (audio)

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnzlfilter.csd
;
; lfilter with an explicit state, as scipy.signal.lfilter(b, a, x, zi=zi):
; the filter starts from zi and hands back its final state zf. Feeding zf in
; as the next zi filters successive blocks as one continuous signal.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    b:CsnArr, a:CsnArr csnbutterba 2, 2000, 0, sr

    ; two blocks of a step, filtered one after the other
    iblock[] = fillarray(1, 1, 1, 1)
    x:CsnArr = csnfromarray(iblock)
    izshape[] = fillarray(2)
    z0:CsnArr = csnzeros(izshape)
    y1:CsnArr, z1:CsnArr csnzlfilter b, a, x, z0
    y2:CsnArr, z2:CsnArr csnzlfilter b, a, x, z1
    csnprint(y1)
    csnprint(y2)
    csnprint(z2)
    turnoff
endin

instr 2
    ; on audio, zf fed back as zi on every control period
    b:CsnArr, a:CsnArr csnbutterba 2, 2000, 0, sr
    izshape[] = fillarray(2)
    z:CsnArr = csnzeros(izshape)
    anoise = noise(0.3, 0)
    afilt, z csnzlfilter b, a, anoise, z
    out afilt
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
i 2 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnlfilter](csnlfilter.md)
* [csnzsosfilter](csnzsosfilter.md)
* [csnbutterba](csnbutterba.md)

## Credits

Pasquale Mainolfi, 2026
