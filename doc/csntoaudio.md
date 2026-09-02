# csntoaudio

## Abstract

Emit an array of ksmps elements as one control period of audio.

## Description

`csntoaudio` is the way out of the array world. It reads a real array of
exactly `ksmps` elements and emits them as one control period of audio.

Only the element count is checked, not the shape: the data is contiguous in
row-major order either way, so a matrix whose elements happen to number `ksmps`
comes out without an intervening flatten.

Samples outside the note's active window are emitted as zero, matching
`csnfromaudio` at the other end.

The source handle is resolved afresh on every control period, so an array that
is freed or replaced under this opcode raises an error instead of reading
memory that no longer belongs to it.

## Syntax

```csound
asig:a = csntoaudio(source:CsnArr)
```

## Arguments

* `source:CsnArr`: a real array holding exactly `ksmps` elements.

## Output

* `asig:a`: the audio signal.

## Execution Time

* Performance (a-rate)

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csntoaudio.csd
;
; csntoaudio is the way out of the array world: it reads a CsnArr of ksmps
; elements and emits them as one control period of audio. Only the element
; count matters, not the shape, so a matrix whose elements happen to number
; ksmps comes out without a flatten.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    shape:k[] init 2
    shape[0] = 4
    shape[1] = 8

    aSig  oscili 0.5, 440
    block:CsnArr = csnfromaudio(aSig)

    ; 32 samples seen as 4 x 8; the data is untouched, only the layout moves
    mat:CsnArr = csnreshape(block, shape)
    aOut  csntoaudio mat

    aErr = abs(aOut - aSig)
    kMax maxk aErr, 1, 1
    if timeinstk() == 40 then
        printf("max error through a 4 x 8 view = %g\n", 1, kMax)
    endif
endin

</CsInstruments>
<CsScore>
i 1 0 0.05
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnfromaudio](csnfromaudio.md)
* [csnunpack](csnunpack.md)
* [csnstream](csnstream.md)

## Credits

Pasquale Mainolfi, 2026
