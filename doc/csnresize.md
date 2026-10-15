# csnresize

## Abstract

Reshape an array to any size, zero-filling what it grows.

## Description

`csnresize` gives an array a new shape without the element-count constraint
[csnreshape](csnreshape.md) imposes. Elements that fit are kept in flat order,
elements the new shape adds are set to zero, and elements it no longer has room
for are dropped.

That makes it the general "make this array that shape" operation: a 4-element
vector resized to 6 comes back as `1 2 3 4 0 0`, and resized to 2 as `1 2`.

Two forms share the name. The one with an output writes a new array and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing. In place, a new shape that fits the source's capacity
reuses its storage; a larger one reallocates, which a
[real-time path](csnrtlock.md) refuses during performance.

A `:CsnStr;` keeps its strings the same way, and the elements the new shape
adds are empty strings, `""`. In place the added elements empty whatever
strings an earlier shrink left past the end and keep their room, so the k form
grows within the capacity the array was reserved with. The output form copies
the strings it keeps into the room its slots already hold; on a [real-time
path](csnrtlock.md) its k form stops with a performance error at the first
string that does not fit. See [string arrays](string-arrays.md).

## Syntax

```csound
out:CsnArr = csnresize(source:CsnArr, shape:i[])
out:CsnArr = csnresize(source:CsnArr, shape:k[])
out:CsnArr = csnresize(source:CsnArr, shape:k[], trig:k)
csnresize(source:CsnArr, shape:i[])
csnresize(source:CsnArr, shape:k[])
csnresize(source:CsnArr, shape:k[], trig:k)
out:CsnStr = csnresize(source:CsnStr, shape:i[])
out:CsnStr = csnresize(source:CsnStr, shape:k[])
out:CsnStr = csnresize(source:CsnStr, shape:k[], trig:k)
csnresize(source:CsnStr, shape:i[])
csnresize(source:CsnStr, shape:k[])
csnresize(source:CsnStr, shape:k[], trig:k)
```

## Arguments

* `source:CsnArr / source:CsnStr`: the array to resize.
* `shape:i[] / shape:k[]`: the new extents, one per dimension. Any element count is accepted.
* `trig:k` (optional): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `out:CsnArr / out:CsnStr`: the resized array. Omit it for the in-place form.

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
; csnresize.csd
;
; csnresize does not require the element counts to match: it keeps what fits,
; zero-fills what it grows, and drops what no longer has room.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr     = csnfromarray(array(1, 2, 3, 4))

    bigger:i[]     = fillarray(6)
    grown:CsnArr   = csnresize(vec, bigger)
    grown_out:i[]  = csntoarray(grown)
    prints("grown  = %g %g %g %g %g %g\n", grown_out[0], grown_out[1], grown_out[2], grown_out[3], grown_out[4], grown_out[5])

    smaller:i[]    = fillarray(2)
    shrunk:CsnArr  = csnresize(vec, smaller)
    shrunk_out:i[] = csntoarray(shrunk)
    prints("shrunk = %g %g\n", shrunk_out[0], shrunk_out[1])

    ; and it can change the rank at the same time
    square:i[]     = fillarray(3, 3)
    mat:CsnArr     = csnresize(vec, square)
    mat_shape:i[]  = csnshape(mat)
    mat_size:i     = csnsize(mat)
    prints("as %g x %g, size = %d\n", mat_shape[0], mat_shape[1], mat_size)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnreshape](csnreshape.md)
* [csntruncate](csntruncate.md)
* [csnpad](csnpad.md)

## Credits

Pasquale Mainolfi, 2026
