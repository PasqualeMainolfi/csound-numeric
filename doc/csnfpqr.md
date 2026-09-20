# csnfpqr

## Abstract

The resonant frequency of a rectangular room for one modal index triple.

## Description

`csnfpqr` answers the frequency of a room mode:

```
f = (c/2) * sqrt((p/L)^2 + (q/W)^2 + (r/H)^2)
```

`L`, `W` and `H` are the room dimensions in metres, `c` the speed of sound in
metres per second and `(p, q, r)` the modal order along each dimension. The
indices are non-negative integers, not flags: `(1,0,0)` is the fundamental along
the length and `(2,0,0)` is the octave above it.

How many indices are non-zero names the mode. One is *axial*, a standing wave
between one opposed pair of walls: these are the strongest and the ones
responsible for a room sounding uneven in the bass. Two is *tangential*,
bouncing off four surfaces, some 3 dB weaker. Three is *oblique*, involving all
six, weaker again.

Below the Schroeder frequency — [csnfschrd](csnfschrd.md) — these resonances are
far enough apart to be heard individually, and the list of them is what room
treatment is aimed at. Above it they overlap into a diffuse field and the
statistical models of [csnt60sab](csnt60sab.md) take over.

Two shapes of input. With a one-dimensional room of exactly three elements
`(L, W, H)` the result is one frequency. With a matrix of `n x 3` rooms, one row
per room, the result is one frequency per row for the same mode — useful for
comparing candidate proportions. The mode is always a one-dimensional array of
three elements.

The dimensions must be greater than zero and the modal indices must be
non-negative integers; both are checked and a violation is an error rather than
a silent NaN. Real only. The speed of sound is an i-rate argument on every
overload: it changes with temperature, but not within a note.

## Syntax

```csound
value:i = csnfpqr(room:CsnArr, mode:CsnArr, c:i)
handle:CsnArr = csnfpqr(rooms:CsnArr, mode:CsnArr, c:i)
value:k = csnfpqr(room:CsnArr, mode:CsnArr, c:i, trig:k)
handle:CsnArr = csnfpqr(rooms:CsnArr, mode:CsnArr, c:i, trig:k)
```

## Arguments

* `room:CsnArr`: the three dimensions `(L, W, H)` in metres, one-dimensional, exactly three elements, all greater than zero.
* `rooms:CsnArr`: `n x 3`, one room per row, for the array form.
* `mode:CsnArr`: the modal orders `(p, q, r)`, one-dimensional, exactly three non-negative integers.
* `c:i`: the speed of sound in metres per second — about 343 at 20 degrees Celsius. Init-time on every overload.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `value:i / value:k`: the modal frequency in hertz, for a single room.
* `handle:CsnArr`: one frequency per room, one-dimensional, for the `n x 3` form.

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
; csnfpqr.csd
;
; The resonant frequencies of a rectangular room. For mode (p, q, r) and
; dimensions (L, W, H) the frequency is (c/2) sqrt((p/L)^2 + (q/W)^2 + (r/H)^2).
; A mode with one non-zero index is axial, two tangential, three oblique; the
; axial ones are the loudest and the ones that make a room sound uneven.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    room:CsnArr     = csnfromarray(array(7, 5, 3))

    ; the three first-order axial modes, one per dimension
    along_l:CsnArr  = csnfromarray(array(1, 0, 0))
    along_w:CsnArr  = csnfromarray(array(0, 1, 0))
    along_h:CsnArr  = csnfromarray(array(0, 0, 1))
    f_l:i           = csnfpqr(room, along_l, 343)
    f_w:i           = csnfpqr(room, along_w, 343)
    f_h:i           = csnfpqr(room, along_h, 343)
    prints("axial L (1,0,0) : %.2f Hz\n", f_l)
    prints("axial W (0,1,0) : %.2f Hz\n", f_w)
    prints("axial H (0,0,1) : %.2f Hz\n", f_h)

    ; the orders are not flags: (2,0,0) is the octave above (1,0,0)
    second:CsnArr   = csnfromarray(array(2, 0, 0))
    f_second:i      = csnfpqr(room, second, 343)
    prints("axial L (2,0,0) : %.2f Hz\n", f_second)

    ; tangential and oblique
    tang:CsnArr     = csnfromarray(array(1, 1, 0))
    obl:CsnArr      = csnfromarray(array(1, 1, 1))
    f_tang:i        = csnfpqr(room, tang, 343)
    f_obl:i         = csnfpqr(room, obl, 343)
    prints("tangential      : %.2f Hz\n", f_tang)
    prints("oblique         : %.2f Hz\n", f_obl)

    ; one row per room: the same mode across three candidate shapes
    shape:i[]       = fillarray(3, 3)
    rooms:CsnArr    = csnreshape(csnfromarray(array(7, 5, 3, 6, 5, 3, 5, 5, 3)), shape)
    modes:CsnArr    = csnfpqr(rooms, along_l, 343)
    csnprint modes

    ; a colder room slows sound down and drops every mode with it
    f_cold:i        = csnfpqr(room, along_l, 331)
    prints("at 331 m/s      : %.2f Hz\n", f_cold)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnfschrd](csnfschrd.md)
* [csnt60sab](csnt60sab.md)
* [csnt60eyr](csnt60eyr.md)

## Credits

Pasquale Mainolfi, 2026
