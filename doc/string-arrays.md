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
- Elementwise strings: [csnstradd](csnstradd.md) joins two arrays or an array
  and an `S`, with broadcasting, [csnstrmul](csnstrmul.md) repeats each
  string a number of times, and [csnstrupper](csnstrupper.md),
  [csnstrlower](csnstrlower.md), [csnstrcapitalize](csnstrcapitalize.md),
  [csnstrswapcase](csnstrswapcase.md) and [csnstrtitle](csnstrtitle.md)
  change the case of the ASCII letters, and [csnstrstrip](csnstrstrip.md),
  [csnstrlstrip](csnstrlstrip.md) and [csnstrrstrip](csnstrrstrip.md) remove
  ASCII whitespace at the ends, or the bytes of a `chars` string, and
  [csnstrcenter](csnstrcenter.md), [csnstrljust](csnstrljust.md) and
  [csnstrrjust](csnstrrjust.md) pad with spaces to a width, and
  [csnstrreplace](csnstrreplace.md) replaces a substring and
  [csnstrsubstr](csnstrsubstr.md) keeps one, as a copy or in place.
- String queries, to a real array in the shape of the source:
  [csnstrlen](csnstrlen.md), [csnstrfind](csnstrfind.md),
  [csnstrrfind](csnstrrfind.md), and the 0/1 masks
  [csnstrstartswith](csnstrstartswith.md),
  [csnstrendswith](csnstrendswith.md), [csnstrisalpha](csnstrisalpha.md),
  [csnstrisdigit](csnstrisdigit.md), [csnstrisalnum](csnstrisalnum.md) and
  [csnstrisspace](csnstrisspace.md), ready for the masks above.
- Conversions: [csnstrtonum](csnstrtonum.md) reads strings as numbers and
  [csnnumtostr](csnnumtostr.md) writes numbers as strings, and
  [csnstrjoin](csnstrjoin.md) joins an array into one `S`.
- From one `S` to an array: [csnstrsplit](csnstrsplit.md) and
  [csnstrrsplit](csnstrrsplit.md) cut it at a separator,
  [csnstrpartition](csnstrpartition.md) and
  [csnstrrpartition](csnstrrpartition.md) into head, separator and tail.
- Rate and real-time marks: [csnk](csnk.md) and [csni](csni.md), in place or
  as a copy, [csnrtlock](csnrtlock.md), [csnrtunlock](csnrtunlock.md).
- Files: [csnsave](csnsave.md), [csnload](csnload.md).

The other elementwise string operations (`zfill`, ...), the other queries
(`count`, comparisons), a format for the conversions, sorting and sets are
not there yet.

## Operators

`+` and `*` work on `:CsnStr;` as [csnstradd](csnstradd.md) and
[csnstrmul](csnstrmul.md) do: `A + B` joins two arrays elementwise with
broadcasting, `A + "x"` and `"x" + A` put a string after or before every
element, and `A * 3` or `3 * A` repeats each string. `X += Y`, `X += "x"`
and `X *= n` update `X`. The other arithmetic operators are not defined on
strings, and an array of counts takes the named form, `csnstrmul(A, counts)`.

The same operators work on one `S`: `S1 + S2` joins, `S * 3` repeats, and
`Sx += Sy`, `Sx *= n` update `Sx`, as [csnstradd](csnstradd.md) and
[csnstrmul](csnstrmul.md) do on strings. Csound itself has no arithmetic on
strings: these operators come with csnum and need it loaded. They run on
every pass, as `strcatk` does.

```csound
SNames[] fillarray "kick", "snare"
names:CsnStr = csnfromarray(SNames)
files:CsnStr = names + ".wav"     // ["kick.wav" "snare.wav"]
marks:CsnStr = 2 * names          // ["kickkick" "snaresnare"]
names += "_01"                    // ["kick_01" "snare_01"]
```

As with numbers, an operator follows its operands' rates: on i-rate arrays it
runs at init, and with a k-rate array, or a k count, on every pass. An `S`
operand has no rate, so the operator keeps a copy of it: on an i-rate array it
runs again at perf whenever the string changes, and its output turns k-rate
from then on. `X += Sv` on an i-rate `X` appends once at init and once more
for each change of `Sv`; on a k-rate `X` it appends on every pass, as `kx +=
1` adds.

## Rate

A string argument has no i and k forms, so an opcode whose only other
arguments are strings cannot be told apart by rate: its k form takes an
explicit trigger, `csnlike(src, "x", ktrig)`, `csnfromarray(Sarr, ktrig)`,
`csnload(path, ktrig)`, `csnstradd(arr, "x", ktrig)`, and the call without one
is the init-time form. The operators take no trigger: see [Operators](#operators).

## Real-time paths

Every element owns a buffer for its string and keeps it. A string is written
into the buffer of the element that takes it, and only a string longer than
the buffer makes the buffer grow; an element emptied by a shrink, a resize or
a reused output keeps its buffer for the next string.

On an array marked for a [real-time path](csnrtlock.md) no buffer may grow
during performance. A k pass that writes strings therefore runs as long as
every string fits the room its element already holds, and the first one that
does not fit stops the pass with a performance error that names the opcode and
both sizes:

```text
[csnarray] A string needs 22 bytes, its slot holds 4, and growing it allocates
at perf time on a real-time path; ...
```

The room comes from the init pass:

- An output marked at init gives each of its elements room for the longest
  string its operands hold there. Slices, takes, layout copies, joins and
  masks only ever place those strings, and the case conversions
  ([csnstrupper](csnstrupper.md), [csnstrlower](csnstrlower.md),
  [csnstrcapitalize](csnstrcapitalize.md),
  [csnstrswapcase](csnstrswapcase.md), [csnstrtitle](csnstrtitle.md)) keep
  their length and the strips ([csnstrstrip](csnstrstrip.md) and its left and
  right forms) and [csnstrsubstr](csnstrsubstr.md) only shorten, so their k
  forms run as long as the operands keep strings no longer than at init.
- The string queries ([csnstrlen](csnstrlen.md), [csnstrfind](csnstrfind.md)
  and the others) write numbers into a real output in the shape of the
  source, so their k forms run as long as the source keeps its shape, as
  does [csnstrtonum](csnstrtonum.md); [csnnumtostr](csnnumtostr.md) gives
  every element room for the longest number at init.
- [csnstrjoin](csnstrjoin.md) joins at init too, and its k form runs while
  the joined string is no longer than that one; the splits
  ([csnstrsplit](csnstrsplit.md) and the others) split at init too, and their
  k forms run while each piece fits the slot of its place at init.
- A form that makes new strings sizes its elements from what it computes at
  init: [csnstradd](csnstradd.md), [csnstrmul](csnstrmul.md) (with a k count,
  from its init value), the paddings ([csnstrcenter](csnstrcenter.md) and its
  left and right forms, with a k width from its init value; in place only
  with a width known at init), [csnstrreplace](csnstrreplace.md), [csnfull](csnfull.md) and [csnlike](csnlike.md) with
  their fill, [csnpad](csnpad.md) with its fill and the source's longest
  string.
- An element set at init, with [csnset](csnset.md), `v[i] = "x"` or
  [csnsetslice](csnsetslice.md), has the room of that string.

So a chain that runs under a deadline is built at init with strings at least
as long as the ones it will write; an array marked later, with
[csnrtlock](csnrtlock.md) after the opcodes that write it, has only the room
its strings took when they were written.

A write in place checks every element before it writes one, so a pass that
stops leaves the array as it was: [csnset](csnset.md) and `v[i] = "x"`,
[csnsetslice](csnsetslice.md), [csnputmask](csnputmask.md),
[csnlike](csnlike.md) and [csnstradd](csnstradd.md) or
[csnstrmul](csnstrmul.md) onto their own operand, and the one-string
[csnpush](csnpush.md) and [csninsert](csninsert.md), which take the element
past the end where a [csnpop](csnpop.md) or [csnremove](csnremove.md) left
the string it dropped. A form with an output stops at the first string that
does not fit, on an output already emptied for the new result; the note stops
with it. An element set to the string it already holds is no write at all, so
a k-rate set of unchanged values is free.

Moving strings costs nothing: reordering in place (`csntranspose`, `csnflip`,
`csnroll`, `csnreverse` without an output, `csnshuffle`), `csnpop`, the
one-string `csnremove` and `csntruncate` in place only move the elements and
their buffers, so they always run on a marked array.

Three k forms stay refused on a marked array, on every pass that writes:
[csnload](csnload.md), which reads new strings from the file, and
[csnpad](csnpad.md) and the block [csninsert](csninsert.md) in place, which
rebuild the array in a working copy whose elements trade places with the
source's on every pass, so no buffer stays where its room was reserved.

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
