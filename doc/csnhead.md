# csnhead

## Abstract

Keep the first n elements of a 1-D array.

## Description

`csnhead` returns the first `n` elements of a 1-D array. It is
[csntruncate](csntruncate.md) restricted to vectors, without an axis argument to
give, and it is the opcode to reach for when an array is being used as a queue or
a ring of samples and only the head matters.

`n` must not exceed the array's element count.

## Syntax

```csound
handle:CsnArr = csnhead(source:CsnArr, n:i)
handle:CsnArr = csnhead(source:CsnArr, n:k)
handle:CsnArr = csnhead(source:CsnArr, n:k, trig:k)
```

## Arguments

* `source:CsnArr`: the 1-D array to read.
* `n:i / n:k`: how many elements to keep, counted from the start.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of a new array holding the first `n` elements.

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
; csnhead.csd
;
; csnhead keeps the front of a vector. Combined with csnroll it reads a sliding
; window off a ring without moving the ring.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr     = csnfromarray(array(10, 20, 30, 40, 50, 60))

    head:CsnArr    = csnhead(vec, 3)
    head_out:i[]   = csntoarray(head)
    n:i            = csnsize(head)
    prints("first %d = %g %g %g\n", n, head_out[0], head_out[1], head_out[2])

    ; roll first, then read the head: a window that slides without copying
    rolled:CsnArr  = csnroll(vec, -2)
    window:CsnArr  = csnhead(rolled, 3)
    window_out:i[] = csntoarray(window)
    prints("window  = %g %g %g\n", window_out[0], window_out[1], window_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csntruncate](csntruncate.md)
* [csngetslice](csngetslice.md)
* [csnroll](csnroll.md)

## Credits

Pasquale Mainolfi, 2026
