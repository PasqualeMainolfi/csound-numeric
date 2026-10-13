# Indexing and slicing with `[]`

`CsnArr` reads and writes single elements with `v[i][j]`, and reads slices with
Csound's slice syntax `v[start:end]`. Both need a Csound 7 build that lets a
plugin type provide `[]` (csound PR #3363, after 7.0.0-beta.18); with an older
Csound they are a compile error and [csnget](csnget.md), [csnset](csnset.md) and
[csngetslice](csngetslice.md) remain the way in.

```csound
iShape[] = fillarray(2, 3)
M:CsnArr = csnreshape(csnarange(0, 6, 1), iShape)  ; [[0, 1, 2], [3, 4, 5]]

iValue = M[1][2]          ; 5
M[0][0] = 10              ; [[10, 1, 2], [3, 4, 5]]

Rows:CsnArr = M[0:0]       ; [[10, 1, 2]]
Cols:CsnArr = M[1:2, 1, 1] ; [[1, 2], [4, 5]]
V:CsnArr    = csnarange(0, 10, 1)
Even:CsnArr = V[0:9, 2]    ; [0, 2, 4, 6, 8]
Tail:CsnArr = V[7:]        ; [7, 8, 9]
```

A [`CsnStr`](string-arrays.md) takes the same syntax: `S1 = names[0][1]`
reads a copy of an element into an `S` variable, `names[0][1] = "x"` and
`names[0][1] init "x"` store a copy, and `names[0:0]` is a `CsnStr` slice that
owns its strings.

## Elements: `v[i][j]`

One index per dimension, in order: `M[1][2]` on a 2-D array, `V[3]` on a 1-D
one. `v[i][j]` is one access with two indexes, not an index into a row, so
fewer indexes than dimensions is an error, never a sub-array; take rows,
columns and ranges with the slice syntax below or with [csngetrow](csngetrow.md),
[csngetcol](csngetcol.md) and [csntake](csntake.md). Each index must be a
finite non-negative integer within its extent.

- `x = v[i][j]` reads a real element.
- `v[i][j] = x` writes one, real or `:Complex;`.
- `v[i][j] init x` writes once, at init, whatever the rate of its arguments.

A complex element is written with `[]` but read with [csnget](csnget.md):
`c:Complex = csnget(v, cell)`. Csound types `v[i]` when it compiles the
instrument, before it knows whether the array is real or complex, so `[]` reads
the real form only, and reading a complex array with it is an error.

### When an access runs

`CsnArr` has no rate of its own, so the arguments decide, as for any variable:

- `v[0] = 7` has only i arguments and writes once, at init, like `ivar = 7`.
- `v[kIdx] = 7` or `v[0] = kValue` writes on every control period, and not at
  init, like `kvar = kValue`. The array is then k-rate: the operators and
  slices that read it follow those writes.
- `kValue = v[0]` reads on every control period and follows the array;
  `iValue = v[0]` reads once, at init.

An i index read passed straight to an opcode with i and k forms, such as
`chnset v[0], "c"`, takes the i form and sends the init value once; read it
into a k variable first to follow it.

## Slices: `v[start:end, step, axis]`

Csound compiles `v[start:end]` to `slicearray`, and csnum provides it for
`CsnArr`. The result is a new array with the source's rank.

- `end` is **included**, as in Csound's own `slicearray`: `V[1:3]` holds 3
  elements. A negative `end` means the last element.
- `V[7:]` runs to the end, `V[:2]` starts at 0.
- `V[0:9, 2]` takes every second element: the number after the comma is the
  step, at least 1.
- `M[1:2, 1, 1]` slices axis 1: the third number is the axis, 0 by default,
  negative from the end.

`end` past the last index is an error, as is an empty range (`end` before
`start`). With i bounds the slice is taken at init and taken again on each
control period while the source is k-rate; with a k bound it is retaken every
period.

What the syntax cannot say:

- One slice per pair of brackets: `M[0:1, 1:2]` and `M[2][1:3]` are syntax
  errors. Chain them, `M[0:1][1:2, 1, 1]`, or use [csngetslice](csngetslice.md).
- A slice is read-only: `V[1:3] = W` is a syntax error. Write a range with
  [csnsetslice](csnsetslice.md).
- An omitted `end` takes no step or axis: `M[1:, 1, 1]` is a syntax error;
  write `M[1:-1, 1, 1]`.

[csngetslice](csngetslice.md) keeps its own convention, `stop` excluded, as in
NumPy.
