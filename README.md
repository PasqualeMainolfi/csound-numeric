# csnum — NumPy-style array opcodes for Csound 7

**NOTE:** *csound-numeric is currently in the testing phase. 
Bug reports, feedback, suggestions, and reports of unexpected behavior 
are very welcome and greatly appreciated, as they help improve the 
library and make it more stable and reliable.*  

`csnum` is a Csound 7 plugin that brings a NumPy-shaped array vocabulary into the
orchestra language: n-dimensional arrays with a shape and strides, elementwise
math, axis-wise reductions, slicing, sorting, statistics, linear-algebra
primitives, interpolation and resampling, about 150 opcodes across some 490
rate and type overloads.

The suite is deliberately narrow: it covers **array work only**. There is no
signal generation, no file I/O, no GUI.

Arrays hold either **real** values (one double per element) or **complex** values
(two doubles per element). Both are first class: creation, conversion, indexing,
reshaping, arithmetic, the reductions and the linear-algebra primitives take
either, and promote a real operand to complex when an operation mixes the two.
Operations with no meaning over the complex field, ordering comparisons,
sorting, rounding, the window functions, interpolation, are real-only and say
so when handed a complex array.

There are **no external dependencies**. The plugin builds from two C11
translation units against the Csound plugin headers and the C standard library, 
nothing else is linked in.

---

## Requirements

- **Csound 7** (API 7.0). The plugin is not built for Csound 6: the opcodes rely
  on Csound 7 user-defined types for their handle and complex arguments.
- A C11 compiler and CMake ≥ 3.16.

## Building

```sh
cmake -S . -B build
cmake --build build
```

CMake looks for `csdl.h` in the usual places (`/usr/local/include/csound`,
`/opt/homebrew/include/csound`, the `CsoundLib64.framework` headers, …). Point it
at a specific tree when needed:

```sh
cmake -S . -B build -DCSOUND_INCLUDE_DIR=/path/to/csound/include
```

When building against a Csound *source* checkout, `version.h` is generated and
may live outside the include directory:

```sh
cmake -S . -B build \
      -DCSOUND_INCLUDE_DIR=/path/to/csound/include \
      -DCSOUND_EXTRA_INCLUDE_DIRS=/path/to/build/include
```

The build produces `libcsnum.dylib` / `.so` / `.dll` in the build directory. Use
it in place with `--opcode-dir`, or install it into Csound's user plugin
directory:

```sh
csound --opcode-dir=build my.csd     # try it without installing
cmake --install build                # ~/Library/csound/7.0/plugins64 (macOS)
                                     # ~/.local/lib/csound/7.0/plugins64 (Linux)
                                     # %LOCALAPPDATA%/csound/7.0/plugins64 (Windows)
```

### Tests

```sh
ctest --test-dir build
```

The suite runs the regression `.csd` files under `tests/` through Csound's
`--run-unit-tests` assertions, plus a static check that every i-time opcode
signature is covered by the i-time regression file.

---

## The model: handles, not Csound arrays

A csnum array lives in a registry owned by the Csound instance. Opcodes exchange
**handles** of type `:CsnArr;`, never Csound `k[]`/`i[]` arrays:

```csound
data:CsnArr = csnfromarray(array(4, 1, 3, 2)) // Csound array  -> handle
back:i[]    = csntoarray(data) // handle -> Csound array
```

`csnfromarray` / `csnfromftable` are the way in, `csntoarray` / `csntoftable` the
way out. Everything in between passes handles, so a chain of twenty operations
copies data zero times more than the operations themselves require.

A handle's array is released when the opcode instance that produced it is
deallocated. Handles declared `@global` outlive their note, that is what makes
them useful across instruments, and are released explicitly:

```csound
shape@global:i[]     = fillarray(1024)
buffer@global:CsnArr = csnzeros(shape)
...
csnfree buffer
```

(In the orchestra header an inline `array(...)` argument is still empty when the
opcode reads it, so bind the shape to a named i-array first, as above.)

Arrays carry up to 8 dimensions and at most 2^28 elements. Shape and strides are
kept in the array, so a reshape or a transpose is a layout change, not a copy.

### Empty arrays

An array with no elements is an ordinary value here, not an error case.
`csnempty` reserves a shape without publishing any element: `csnsize` reports 0
and `csnisempty` reports 1, while `csnshape` still reports the extents that were
reserved. That reservation is the capacity `csnpush` fills, so an array built up
element by element only reallocates when it outgrows what was reserved.

```csound
cap@global:i[] = fillarray(4)
buf:CsnArr     = csnempty(giCap) // size 0, room for 4
csnpush(buf, 10)
csnpush(buf, 20) // size 2
last:i = csnpop(buf) // 20, size back to 1
```

Empty arrays travel through the rest of the suite instead of stopping it: the
shape transforms return an empty result of the right rank, concatenation with an
empty operand yields the other one, and `csnsum` over an empty array is 0 rather
than an error. The item type is an i-argument, so an array can also be declared
empty *and* complex from the start with `csnempty(cap, 1)`.

---

## What is covered

Grouped by what they do, rather than listed one by one. The full list, with a
one-line description and the rates each opcode supports, is in
[`OPS_INDEX.md`](OPS_INDEX.md).

- **Creation**: empty / zeros / ones / full / identity, `csnlike` to build one
  shaped like an array you already have, `arange`, `linspace`, `logspace`,
  `geomspace`, seeded random arrays.
- **Conversion and lifetime**: to and from Csound arrays and function tables,
  copy, free, type and shape queries.
- **Shape and layout**: reshape, flatten, transpose, flip, roll, pad, truncate,
  head, resize, concat, insert, remove, push, pop.
- **Indexing**: element get/set, slices, gathers, and the index-returning
  searches (`argwhere`, `argnonzero`, `argisnan`).
- **Elementwise math**: the four operations plus power, log, divmod, hypot; the
  usual transcendental and rounding functions; degree/radian conversion; phase
  wrap and unwrap.
- **Comparison and logic**: the six comparisons, logical and/or/not, counters
  (`cnteq`, `cntnz`, `cntnan`), `all` / `any`.
- **Reductions and statistics**: sum, prod, mean, min, max, median, variance,
  standard deviation, percentile, quantile, argmin/argmax, cumulative sums and
  products, differences, gradient, and moving-window statistics.
- **Sorting and sets**: sort, argsort, unique, argunique.
- **Linear algebra and geometry**: dot, inner, outer, matmul, trace, diagonal,
  norms, normalize, cross product, distances, angular distance, vector
  projection and rejection, reflection.
- **Complex**: real / imaginary parts, angle, conjugate, conversion to and from
  real arrays.
- **Interpolation and resampling**: `csninterp` (linear, nearest, previous,
  next, monotone cubic PCHIP, with error / clamp / fill / extrapolate boundary
  policies) and `csnresample`.
- **Windows**: Hann, Hamming, Bartlett, Blackman, Kaiser.
- **Persistence**: `csnsave` and `csnload` write an array to a `.csn` file and
  read it back, shape and element type included.

### Conventions shared by the whole suite

- **Axis argument.** Opcodes that can work along one axis take an optional axis;
  `-1` (the default) means "the whole array, read flat".
- **In-place forms.** Where it makes sense, the opcode that publishes a new
  handle also has a sibling under the same name that writes back into its source
  and returns nothing: `csnnormalize(data)` normalizes in place, while
  `Norm:CsnArr = csnnormalize(data)` leaves the source alone.
- **Rate overloads.** The i-rate and k-rate forms share a name; Csound picks the
  overload from the rate of the arguments you pass.
- **Trigger.** k-rate forms take an optional trailing trigger. A zero trigger
  skips the pass entirely and republishes the previous result. `csnsave` and
  `csnload` are the exception: there is no previous result to republish, so a
  zero trigger simply does not touch the disk.

---

## k-rate performance: array versioning

The interesting part of csnum is what happens when these opcodes run inside the
k-rate loop, thousands of times per second, on arrays that usually have not
changed since the last pass.

Every array carries four counters, data, shape, ndim and item type, bumped only
by a writer that actually changed that aspect. A k-rate opcode records the
version of its source, and of the slot it publishes, at the end of a pass that
did real work. On the next pass it compares:

- the **source** version, plus the source handle itself, so a recycled slot is
  never mistaken for the same array;
- the **output slot** version, so an opcode that finds its own result untouched
  can republish the handle instead of recomputing it;
- the **request**, shape, ndim, item type and any scalar parameter it depends
  on, such as an axis or a window size.

If all of them match, the opcode publishes last pass's handle and returns. No
allocation, no copy, no arithmetic. A chain of csnum opcodes driven by a source
that moves once every few hundred k-cycles therefore costs almost nothing on the
passes in between, and the saving compounds along the chain: a consumer sees its
own input as unchanged precisely because the producer upstream skipped its work.

Three details make this safe rather than merely fast:

- **The output slot is reused, not reallocated.** A k-rate producer owns its
  destination slot for the life of the note and only resizes it when the
  requested shape actually changes.
- **In-place opcodes publish their write.** They bump the data version *and*
  record it as their own, so the next pass recognizes its own handiwork and
  leaves it alone, while every other consumer still sees a new generation.
- **Self-aliasing is rejected.** An opcode whose output handle is fed back as its
  own input is refused at init rather than silently reading a buffer it is in the
  middle of rewriting; the in-place overloads exist for that case and use their
  own scratch buffer.

Scratch buffers are per-opcode-instance and grow geometrically, so a k-rate pass
allocates nothing in steady state.

---

## Saving and loading

`csnsave` and `csnload` move an array to and from a `.csn` file. The path must
carry that extension; anything else is refused before a file is opened.

The format is a fixed 64-byte header followed by the raw payload: a `CSDN`
magic, a major and minor version, the element type, the dimension count, the
element count, the shape, and the payload length in bytes. Everything that
matters is therefore restored, not inferred — a 2×3 array comes back 2×3, and a
complex array comes back complex rather than as twice as many reals.

Every field is validated on the way in. A truncated file, a shape whose element
count contradicts the declared payload length, an unknown element type, or a
version this build does not know are all rejected with a message naming the
field, rather than producing a plausible-looking array from garbage.

At k-rate the trigger is the whole contract: it fires, the file is read. There
is deliberately no caching between triggers, not even on an unchanged path.
`csnload` reads a file it does not own, so the path proves nothing about the
bytes behind it, and a stat-based stamp would only narrow the window — on HFS+,
SMB/NFS and FAT the mtime granularity is one to two seconds, wide enough for a
same-size rewrite to hide in. Re-reading a small file already in the page cache
is cheap; silently handing back stale data is not.

```csound
data:CsnArr = csnfromarray(array(1, 2, 3, 4, 5, 6))
csnsave(data, "analysis.csn")

back:CsnArr = csnload("analysis.csn")
values:i[]  = csntoarray(back) // 1 2 3 4 5 6
```

At k-rate, reloading a file another process keeps rewriting. Until the first
trigger fires the handle still holds the empty array the init pass published,
so a consumer that cannot read an empty extent belongs behind the trigger too:

```csound
instr 1
    trig:k       = metro(10)
    table:CsnArr = csnload("live.csn", trig)
    n:k          = csnsize(table)
endin
```

---

## Examples

Sort and reduce, entirely at i-time:

```csound
data:CsnArr     = csnfromarray(array(4, 1, 3, 2))
sorted:CsnArr   = csnsort(data)
sorted_back:i[] = csntoarray(sorted) // 1 2 3 4
peak:i          = csnmax(Data) // 4
```

Reshape to 2×3 and reduce along an axis:

```csound
shape:i[]   = fillarray(2, 3)
mat:CsnArr  = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
cols:CsnArr = csnsum(mat, 0) // 5 7 9 (one value per column)
```

Complex arrays use the same opcodes, and a single element reads back as a
`:Complex;`:

```csound
z:CsnArr    = csntocomplex(data)
conj:CsnArr = csnconj(z)
cell:i[]    = fillarray(0)
w:Complex   = csnget(conj, cell)
```

Interpolate one value on a breakpoint table, then resample a curve:

```csound
X:CsnArr  = csnfromarray(array(0, 1, 2, 3))
Y:CsnArr  = csnfromarray(array(0, 10, 20, 30))
at:i      = csninterp(1.5, X, Y, 0, 1) // 15 (linear, clamped at the ends)
up:CsnArr = csnresample(Y, 7, 0, 1) // 0 5 10 15 20 25 30
```

At k-rate, with a trigger: the work runs on the triggered passes, and the
opcodes republish their previous result on all the others.

```csound
instr 1
    trig:k        = metro(200)
    scaled:CsnArr = csnmul(buffer, 0.5, trig)
    peak:k        = csnmax(scaled, trig)
    printf("peak=%.1f\n", trig, peak)
endin
```

In place, when a new handle would be waste:

```csound
csnnormalize(data) // rewrites Data; downstream consumers see a new generation
```
