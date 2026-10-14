# String arrays

A `CsnStr` is an array of strings. It has the layout of a `CsnArr`, up to 8
dimensions, a shape and C-order strides, and it is a value in the same way:
`B = A` copies, and the strings are released with the variable that owns
them. Every element owns its string, so a copy, a slice or a reload never
shares one with its source.

It is a type of its own rather than a third element type of `CsnArr`. The
numeric opcodes are declared on `:CsnArr;`, so handing them a `CsnStr` is
refused when the orchestra is compiled instead of reaching code that reads
numbers; each opcode that also makes sense on strings has a `:CsnStr;`
overload, and [csntype](csntype.md) reports `2` for one.

## What takes a `CsnStr`

- Creation: [csnempty](csnempty.md), [csnfull](csnfull.md),
  [csnlike](csnlike.md), [csnfromarray](csnfromarray.md) from an `S[]`.
- Out: [csntoarray](csntoarray.md) to an `S[]`, [csnprint](csnprint.md).
- Elements: [csnget](csnget.md), [csnset](csnset.md), `v[i][j]`,
  `v[i][j] = "x"`, `v[i][j] init "x"`.
- Slices: [csngetslice](csngetslice.md) and `v[start:end, step, axis]`,
  [csnsetslice](csnsetslice.md), [csntake](csntake.md) along an axis or
  flat to an `S`.
- Queries: [csnsize](csnsize.md), [csndims](csndims.md),
  [csnshape](csnshape.md), [csnisempty](csnisempty.md), [csntype](csntype.md).
- Layout, as a copy or in place: [csnreshape](csnreshape.md),
  [csnflatten](csnflatten.md), [csntranspose](csntranspose.md),
  [csnflip](csnflip.md), [csnroll](csnroll.md), [csnreverse](csnreverse.md).
- Joining and growing: [csnconcat](csnconcat.md) of two `CsnStr`,
  [csnstack](csnstack.md) of two or more, [csnpad](csnpad.md) with a string
  fill value, as a copy or in place.
- Growing and shrinking in place: [csnpush](csnpush.md),
  [csnpop](csnpop.md), [csninsert](csninsert.md) and
  [csnremove](csnremove.md), one string or a block along an axis.
- Cutting and resizing, as a copy or in place: [csntruncate](csntruncate.md),
  [csnhead](csnhead.md), [csnresize](csnresize.md), which fills what it adds
  with `""`.
- Copies and order: [csncopy](csncopy.md), [csnshuffle](csnshuffle.md).
- Masks, with a real mask: [csnwhere](csnwhere.md),
  [csnputmask](csnputmask.md) as `csnputmask(dest, mask, values)`,
  [csncompress](csncompress.md), [csnselect](csnselect.md).
- Rate and real-time marks: [csnk](csnk.md) and [csni](csni.md), in place or
  as a copy, [csnrtlock](csnrtlock.md), [csnrtunlock](csnrtunlock.md).
- Files: [csnsave](csnsave.md), [csnload](csnload.md).

The elementwise string operations (`np.strings.add`, `upper`, `strip`, ...),
queries, conversions, sorting and sets are not there yet.

## Rate and real-time paths

A string argument has no i and k forms, so an opcode whose only other
arguments are strings cannot be told apart by rate: its k form takes an
explicit trigger, `csnlike(src, "x", ktrig)`, `csnfromarray(Sarr, ktrig)`,
`csnload(path, ktrig)`, and the call without one is the init-time form.

Writing a string allocates. A k-rate pass that writes strings is therefore
refused on an array marked for a [real-time path](csnrtlock.md), with a message
that names the opcode; build the array at init instead, or clear the mark. An
element set to the string it already holds is no write and allocates nothing,
so a k-rate `csnset`, `csnsetslice` or `csnputmask` of unchanged values is
free. Reordering in place (`csntranspose`, `csnflip`, `csnroll`, `csnreverse`
without an output, `csnshuffle`) only moves the strings the array already
holds, so it allocates nothing and is allowed there; the k forms that return a
copy are refused like any other writer, and so is `csnpad` in place, which
allocates its fill strings. For the same reason `csnpop`, the one-string
`csnremove` and `csntruncate` in place are allowed: the strings they drop stay
with the array past its end, and a later push or insert takes the slot back.
`csnpush` and `csninsert` store a copy, and `csnresize` in place releases the
strings left past the end as it grows over them, so their k forms are refused.

## Files

[csnsave](csnsave.md) writes a `.npy` as a fixed-width `<U` array: Csound
strings are UTF-8 bytes, decoded to the code points NumPy stores and padded to
the longest element, so `np.load` returns a `str` array. A byte that does not
start a valid UTF-8 sequence is taken as its own code point, so any string can
be saved. A `.csn` file stores each element as a byte count and its bytes,
under dtype `3`.

[csnload](csnload.md) reads a `.npy` of dtype `<U` (re-encoded as UTF-8) or
`|S` (bytes as they are), in either byte order and in C or Fortran order, and a
`.csn` of dtype `3`. An element ends at its first zero, where NumPy's padding
starts. A file of numbers loaded into a `:CsnStr;`, or a file of strings into a
`:CsnArr;`, is refused with a message saying so.

## Example

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; string-arrays.csd
;
; A CsnStr is an array of strings with the layout of a CsnArr. It comes in
; from an S[], is read and written with [], sliced, saved as a NumPy <U array
; and loaded back, and goes out as an S[] again.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    labels:S[]   = fillarray("kick", "snare", "hat", "tom", "crash", "ride")
    grid:CsnStr  = csnfromarray(labels)
    shape:i[]    = fillarray(2, 3)
    csnreshape(grid, shape)                 ; in place: a layout change only
    csnprint(grid)

    grid[1][2]   = "china"                  ; [] stores a copy of the string
    second:S     = grid[0][1]
    kind:i       = csntype(grid)
    count:i      = csnsize(grid)
    prints("grid[0][1] = %s, csntype = %d, csnsize = %d\n", second, kind, count)

    top:CsnStr   = grid[0:0]                ; first row, end included
    top[0][0]    = "808"                    ; the slice owns its strings
    still:S      = grid[0][0]
    changed:S    = top[0][0]
    prints("slice[0][0] = %s, grid[0][0] is still %s\n", changed, still)

    csnsave(grid, "string_arrays_example.npy")   ; np.load gives dtype <U5
    back:CsnStr  = csnload("string_arrays_example.npy")
    rows:S[]     = init(2, 3)               ; an array of S, given its rank
    rows         = csntoarray(back)
    last:S       = rows[1][2]
    prints("loaded back, rows[1][2] = %s\n", last)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
CsnStr(shape=(2, 3), dtype=str)
[['kick' 'snare' 'hat']
 ['tom' 'crash' 'ride']]
grid[0][1] = snare, csntype = 2, csnsize = 6
slice[0][0] = 808, grid[0][0] is still kick
loaded back, rows[1][2] = china
```

`rows:S[]` declares an array of strings; its rank comes from what sizes it.
For a multi-dimensional `csntoarray` output, give it the rank with `init`
first, as above, or use the older spelling with one bracket pair per
dimension, `Srows[][] = csntoarray(back)`.
