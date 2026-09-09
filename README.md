# csnum: numpy-style array opcodes for Csound 7

**NOTE:** *csound-numeric is currently in the testing phase.
Bug reports, feedback, suggestions, and reports of unexpected behavior
are very welcome and greatly appreciated, as they help improve the
library and make it more stable and reliable.*

`csnum` is a Csound 7 plugin that brings a numpy-shaped array vocabulary into the
orchestra language: n-dimensional arrays with a shape and strides, elementwise
math, axis-wise reductions, slicing, sorting, statistics, linear-algebra
primitives, convolution, interpolation and resampling, 210 opcodes across 618
rate and type overloads.

The suite is deliberately narrow: it covers **array work only**. There is no
signal generation and no GUI. Two doors lead out of that: `csnsave` / `csnload`
persist an array to disk, and the audio bridge moves blocks of samples between
Csound's audio signals and csnum arrays, so an analysis chain can be written
with array operations and sent back out as sound.

Arrays hold either **real** values (one double per element) or **complex** values
(two doubles per element). Both are first class: creation, conversion, indexing,
reshaping, arithmetic, the reductions and the linear-algebra primitives take
either, and promote a real operand to complex when an operation mixes the two.
Operations with no meaning over the complex field, ordering comparisons,
sorting, rounding, the window functions, interpolation, are real-only and say
so when handed a complex array.

There are **no external dependencies**. The plugin builds from five C11
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
way out; on the audio path the pairs are `csnfromaudio` / `csntoaudio` and
`csnpack` / `csnunpack`. Everything in between passes handles, so a chain of
twenty operations copies data zero times more than the operations themselves
require.

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

### Set arrays and their invariant

A set array is a one-dimensional real array kept in ascending, duplicate-free
form. `csnlikeset` establishes that invariant; the set operations and the
in-place `csnsetinsert` / `csnsetremove` operations preserve it.

Generic array operations do **not** guarantee the set invariant. In particular,
a generic in-place write or transformation may leave the values unsorted or
duplicated. A later set operation rejects such an array rather than silently
using binary search on invalid data. Pass the modified array through
`csnlikeset` again to produce a normalized set. `csnunlikeset` instead removes
the set classification when ordinary array semantics are intended. See
[Set arrays and their invariant](doc/set-arrays.md) for the complete contract.

```csound
a:CsnArr = csnlikeset(csnfromarray(array(3, 1, 2, 2)))
b:CsnArr = csnlikeset(csnfromarray(array(2, 4)))
both:CsnArr = csnsetunion(a, b)       // [1, 2, 3, 4]
csnsetinsert both, 5                  // [1, 2, 3, 4, 5]
has3:i = csnsetcontains(both, 3)      // 1
```

The API also provides intersection, difference, symmetric difference, removal,
and subset, superset, disjointness, and equality predicates. Each opcode has a
runnable example in the [set-operation reference](doc/README.md#sorting-and-sets).

### Printing arrays

`csnprint` writes the shape, element type and values directly to Csound's message
stream. Values use five significant digits and nested arrays follow numpy's
bracket and indentation style:

```csound
values:i[] = fillarray(1.234567, 2, 3, 4)
shape:i[]  = fillarray(2, 2)
mat:CsnArr = csnreshape(csnfromarray(values), shape)
csnprint(mat)
```

```text
CsnArr(shape=(2, 2), dtype=float64)
[[1.2346 2]
 [3 4]]
```

Arrays with more than 1000 elements are summarized with their first and last
three entries along every long dimension. The k-rate form,
`csnprint(handle, trig)`, prints on a non-zero trigger and emits nothing when the
trigger is zero. See the full [csnprint reference](doc/csnprint.md).

---

## What is covered

Grouped by what they do, rather than listed one by one. The full list, with a
one-line description and the rates each opcode supports, is in
[`OPS_INDEX.md`](OPS_INDEX.md); one page per opcode, with every overload, the
meaning of each argument and a runnable example, is under
[`doc/`](doc/README.md). Coming from NumPy, the table in
[NumPy correspondence](#numpy-correspondence) maps every opcode to the call it
stands for. The examples are also standalone `.csd` files in
[`example/`](example), and all of them run:

```sh
csound --opcode-dir=build example/csnsort.csd
```

- **Creation**: empty / zeros / ones / full / identity, `csnlike` to build one
  shaped like an array you already have, `arange`, `linspace`, `logspace`,
  `geomspace`, seeded random arrays.
- **Conversion, lifetime and inspection**: to and from Csound arrays and
  function tables, copy, free, type and shape queries, and numpy-style printing.
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
- **Sorting and sets**: sort, argsort, unique, argunique, set conversion,
  membership, union, intersection, difference and set predicates.
- **Linear algebra and geometry**: dot, inner, outer, matmul, trace, diagonal,
  norms, normalize, cross product, distances, angular distance, vector
  projection and rejection, reflection.
- **Complex**: real / imaginary parts, angle, conjugate, conversion to and from
  real arrays.
- **Fourier analysis**: full and real FFT/IFFT along one axis or across a 2-D
  matrix, STFT/ISTFT, frequency-coordinate arrays, and FFT shift/unshift.
- **Convolution and correlation**: convolution and cross-correlation, with a
  1-D kernel flat or along one axis, or with a kernel shaped like the source,
  over NumPy's FULL / SAME / VALID spans — computed directly, or through
  Fourier transforms for the kernels long enough to make that pay.
- **Interpolation and resampling**: `csninterp` (linear, nearest, previous,
  next, monotone cubic PCHIP, with error / clamp / fill / extrapolate boundary
  policies) and `csnresample`.
- **Windows**: Hann, Hamming, Bartlett, Blackman, Kaiser.
- **Persistence**: `csnsave` and `csnload` write an array to a `.csn` file and
  read it back, shape and element type included.
- **Audio bridge**: `csnfromaudio` / `csntoaudio` move one control period between
  an audio signal and an array, `csnpack` / `csnunpack` do the same for a whole
  `a[]` as a `channels x ksmps` matrix, and `csnsnap` / `csnstream` slice a stream
  into overlapping frames of a size independent of `ksmps` and overlap-add them
  back. Arrays on an audio path refuse to reallocate during performance.

### Conventions shared by the whole suite

- **Axis argument.** Opcodes that can work along one axis take an optional axis;
  `-1` (the default) means "the whole array, read flat".
- **In-place forms.** Where it makes sense, the opcode that publishes a new
  handle also has a sibling under the same name that writes back into its source
  and returns nothing: `csnnormalize(data)` normalizes in place, while
  `Norm:CsnArr = csnnormalize(data)` leaves the source alone.
- **Rate overloads.** The i-rate and k-rate forms share a name; Csound picks the
  overload from the rate of the arguments you pass.
- **Trigger.** Most k-rate forms take an optional trailing trigger. A zero
  trigger skips the pass entirely and republishes the previous result. Where
  the trigger is the only k-rate argument, as in `csnprint`, it is required so
  Csound can distinguish the performance overload from the init-time one.
  `csnprint`, `csnsave` and `csnload` have no previous computed result to
  republish, so a zero trigger simply performs no side effect.
- **Realtime paths.** The opcodes that bring audio in take an optional trailing
  `irt`, 1 by default. It marks the array they publish as belonging to a
  realtime path, and the mark travels to every array derived from it. A marked
  array refuses to reallocate during performance and names the variable in the
  error, because a malloc on the audio thread is what a dropout sounds like.
  Pass `irt = 0` where the frames are being harvested for analysis rather than
  sent back out. `csnrtlock` sets the same mark on any handle, for chains that
  run under a deadline without touching audio; `csnrtunlock` clears it on a
  selected branch. Both have init and triggered k-rate forms. The state is
  inherited when a derived array is created, so changing a source does not
  retroactively change existing descendants.

---

## NumPy correspondence

The names follow NumPy wherever the operation is the same one. The table below
maps every opcode to the NumPy call it stands for, family by family, so a
function you already know can be found under its Csound name. The mapping is by
*intent*, not by signature: csnum opcodes exchange handles, take their shape
arguments as Csound i-arrays, and have no keyword arguments, so the NumPy column
is what the opcode computes, not a transliteration of its call.

Four differences apply throughout and are not repeated in every row:

- **Axis.** The optional axis defaults to `-1`, meaning "read the whole array
  flat". NumPy spells that `axis=None`; NumPy's own `-1` means the last axis.
- **Booleans.** csnum has no boolean element type. The comparisons, the `is*`
  predicates and the logical operations return a real array of `0` and `1` where
  NumPy returns `bool_`, and any of them is accepted as a mask.
- **Index results.** `csnargmin`, `csnargmax`, `csnargsort`, `csnargwhere` and
  friends return **coordinates**, one row per result with one column per
  dimension, where NumPy returns flat indices unless you call
  `np.unravel_index`.
- **Broadcasting.** There is none. Binary opcodes take two arrays of the same
  shape, or an array and a scalar. `csnstack` likewise requires equal shapes.

An em dash in the NumPy column means there is no NumPy counterpart; a `scipy.`
entry means the operation lives outside NumPy proper.

### Creation

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csnempty` | `np.empty` | Reserves capacity: size is 0, not the reserved extent. `csnpush` fills it. |
| `csnzeros` | `np.zeros` | |
| `csnones` | `np.ones` | |
| `csnfull` | `np.full` | |
| `csnlike` | `np.full_like` | Also covers `zeros_like` / `ones_like` via the fill value. |
| `csnidentity` | `np.identity` | |
| `csnarange` | `np.arange` | |
| `csnlinspace` | `np.linspace` | |
| `csnlogspace` | `np.logspace` | |
| `csngeomspace` | `np.geomspace` | |
| `csnrand` | `np.random.uniform` | |
| `csnrandint` | `np.random.randint` | |
| `csnseed` | `np.random.seed` | |

### Conversion, lifetime and queries

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csnfromarray` | `np.array` | From a Csound `i[]` / `k[]`. |
| `csntoarray` | `ndarray.tolist` | The only way back out to a Csound array. |
| `csnfromftable` | — | Csound function table in. |
| `csntoftable` | — | Csound function table out. |
| `csncopy` | `np.copy` | |
| `csnfree` | `del` | Explicit release; only needed for `@global` handles. |
| `csntype` | `ndarray.dtype` | `0` real, `1` complex. |
| `csndims` | `ndarray.ndim` | |
| `csnsize` | `ndarray.size` | |
| `csnshape` | `ndarray.shape` | |
| `csnisempty` | `a.size == 0` | |
| `csnprint` | `print(a)` | Same bracket and indentation layout, same 1000-element summarization. |

### Persistence

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csnsave` | `np.save` | Own `.csn` container, not `.npy`. |
| `csnload` | `np.load` | |

### Shape and layout

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csnreshape` | `np.reshape` | |
| `csnflatten` | `np.ravel` | |
| `csntranspose` | `np.transpose` | Layout change, no copy. |
| `csnflip` | `np.flip` | |
| `csnroll` | `np.roll` | |
| `csnreverse` | `a[::-1]` | Flat element order. |
| `csnpad` | `np.pad` | Constant fill only. |
| `csntruncate` | `a[:n]` | One axis or every axis. |
| `csnhead` | `a[:n]` | 1-D only. |
| `csnresize` | `ndarray.resize` | Zero-fills what it grows, like the method. `np.resize` repeats instead. |
| `csnconcat` | `np.concatenate` | |
| `csnstack` | `np.stack` | |
| `csninsert` | `np.insert` | |
| `csnremove` | `np.delete` | |
| `csnpush` | `np.append` | In place, into reserved capacity. |
| `csnpop` | `a[-1]` + `np.delete` | Returns the element and shortens the array. |

### Indexing and selection

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csnget` | `a[i, j]` | Coordinates come in as an i-array. |
| `csnset` | `a[i, j] = v` | |
| `csngetrow` | `a[i, :]` | |
| `csngetcol` | `a[:, j]` | |
| `csngetslice` | `a[start:stop:step]` | Along one axis. |
| `csnsetslice` | `a[start:stop:step] = v` | |
| `csntake` | `np.take` | Drops the indexed axis. |
| `csnwhere` | `np.where` | |
| `csnputmask` | `np.putmask` | |
| `csncompress` | `np.compress` | |
| `csnselect` | `np.extract` | `a[mask]`, flattened. |
| `csnargwhere` | `np.argwhere(np.isin(a, v))` | Coordinates of the elements matching a value array. |
| `csnargnonzero` | `np.argwhere(a)` | |
| `csnargisnan` | `np.argwhere(np.isnan(a))` | |

### Elementwise arithmetic

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csnadd` | `np.add` | |
| `csnsubtract` | `np.subtract` | |
| `csnmul` | `np.multiply` | |
| `csndiv` | `np.divide` | |
| `csnpow` | `np.power` | |
| `csnlog` | `np.log(a) / np.log(base)` | Arbitrary base in one call. |
| `csndivmod` | `np.divmod` | Two handles out. |
| `csnhypot` | `np.hypot` | |
| `csnminimum` | `np.minimum` | |
| `csnmaximum` | `np.maximum` | |
| `csnclip` | `np.clip` | |
| `csnabs` | `np.abs` | Magnitude for complex. |
| `csnsign` | `np.sign` | |
| `csnfloor` | `np.floor` | |
| `csnceil` | `np.ceil` | |
| `csnround` | `np.round` | |

### Transcendental functions

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csnexp` | `np.exp` | |
| `csnsqrt` | `np.sqrt` | |
| `csncbrt` | `np.cbrt` | |
| `csnsin` `csncos` `csntan` | `np.sin` `np.cos` `np.tan` | |
| `csnasin` `csnacos` `csnatan` | `np.arcsin` `np.arccos` `np.arctan` | |
| `csnsinh` `csncosh` `csntanh` | `np.sinh` `np.cosh` `np.tanh` | |
| `csnasinh` `csnacosh` `csnatanh` | `np.arcsinh` `np.arccosh` `np.arctanh` | |

### Angles and phase

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csndegtorad` | `np.deg2rad` | |
| `csnradtodeg` | `np.rad2deg` | |
| `csnatan2` | `np.arctan2` | |
| `csnwrap` | `np.mod` | Folds into one period centred on zero, `[-period/2, period/2)`. |
| `csnunwrap` | `np.unwrap` | |

### Comparison and logic

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csngt` | `np.greater` | 0/1 real array out. |
| `csnlt` | `np.less` | |
| `csnge` | `np.greater_equal` | |
| `csnle` | `np.less_equal` | |
| `csneq` | `np.equal` | |
| `csnne` | `np.not_equal` | |
| `csnisnan` | `np.isnan` | |
| `csnisinf` | `np.isinf` | |
| `csnisfin` | `np.isfinite` | |
| `csnlogicand` | `np.logical_and` | |
| `csnlogicor` | `np.logical_or` | |
| `csnlogicnot` | `np.logical_not` | |
| `csnall` | `np.all` | |
| `csnany` | `np.any` | |
| `csncnteq` | `np.count_nonzero(a == v)` | |
| `csncntnz` | `np.count_nonzero` | |
| `csncntnan` | `np.count_nonzero(np.isnan(a))` | |

### Reductions and statistics

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csnsum` | `np.sum` | |
| `csnprod` | `np.prod` | |
| `csnsub` | `np.subtract.reduce` | Every element subtracted from the first. |
| `csnmean` | `np.mean` | |
| `csnmin` | `np.min` | |
| `csnmax` | `np.max` | |
| `csnmedian` | `np.median` | |
| `csnrms` | `np.sqrt(np.mean(a ** 2))` | |
| `csnstd` | `np.std` | |
| `csnvar` | `np.var` | |
| `csnpercentile` | `np.percentile` | |
| `csnquantile` | `np.quantile` | |
| `csnargmin` | `np.argmin` | Coordinates, not a flat index. |
| `csnargmax` | `np.argmax` | Coordinates, not a flat index. |
| `csncumsum` | `np.cumsum` | |
| `csncumprod` | `np.cumprod` | |
| `csndiff` | `np.diff` | |
| `csngrad` | `np.gradient` | |
| `csnmovmean` | — | `pandas.Series.rolling(w).mean()`; in NumPy, `np.convolve` with a box. |
| `csnmovmedian` | — | `rolling(w).median()`. |
| `csnmovmin` | — | `rolling(w).min()`. |
| `csnmovmax` | — | `rolling(w).max()`. |
| `csnmovstd` | — | `rolling(w).std()`. |
| `csnmovvar` | — | `rolling(w).var()`. |

### Sorting and sets

NumPy's set functions take any array and sort it on the way through. csnum
splits that in two: `csnlikeset` normalizes once and marks the array, and the
set operations then require the mark, so they can rely on binary search instead
of re-sorting on every k-rate pass.

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csnshuffle` | `np.random.shuffle` | In place. |
| `csnsort` | `np.sort` | |
| `csnargsort` | `np.argsort` | Coordinates. |
| `csnunique` | `np.unique` | |
| `csnargunique` | `np.unique(a, return_index=True)` | |
| `csnlikeset` | `np.unique` | Plus the set mark the operations below require. |
| `csnunlikeset` | — | Drops the mark, keeps the values. |
| `csnsetinsert` | — | In place, invariant preserving. |
| `csnsetremove` | — | In place, invariant preserving. |
| `csnsetcontains` | `np.isin` | Single value. |
| `csnsetunion` | `np.union1d` | |
| `csnsetintersect` | `np.intersect1d` | |
| `csnsetdiff` | `np.setdiff1d` | |
| `csnsetsymdiff` | `np.setxor1d` | |
| `csnsetissubset` | `np.isin(a, b).all()` | |
| `csnsetissuperset` | `np.isin(b, a).all()` | |
| `csnsetisdisjoint` | `np.intersect1d(a, b).size == 0` | |
| `csnsetisequal` | `np.array_equal(a, b)` | On normalized sets. |

### Linear algebra and geometry

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csndot` | `np.dot` | |
| `csninner` | `np.inner` | |
| `csnouter` | `np.outer` | |
| `csnmatmul` | `np.matmul` | |
| `csntrace` | `np.trace` | |
| `csndiag` | `np.diag` | Both directions, as in NumPy. |
| `csnnorm` | `np.linalg.norm` | |
| `csnnormalize` | `a / np.linalg.norm(a, ord)` | Default order is the sum of magnitudes. |
| `csncross` | `np.cross` | 3-element vectors. |
| `csndist` | `np.linalg.norm(a - b, ord)` | Minkowski distance. |
| `csnpairdist` | `np.abs(a - b)` | Elementwise, same shape. |
| `csnangledist` | — | `arccos(dot(a, b) / (norm(a) * norm(b)))`. |
| `csnproject` | — | `dot(a, b) / dot(b, b) * b`. |
| `csnreject` | — | `a - project(a, b)`. |
| `csnreflect` | — | `a - 2 * project(a, b)`. |

### Complex arrays

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csnreal` | `np.real` | |
| `csnimag` | `np.imag` | |
| `csnangle` | `np.angle` | |
| `csnconj` | `np.conj` | |
| `csntocomplex` | `a.astype(complex)` | |
| `csntoreal` | `a.real` | Keeps the real parts. |

### Fourier analysis

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csnfft` | `np.fft.fft` | Length is a fixed i-rate power of two. |
| `csnifft` | `np.fft.ifft` | |
| `csnrfft` | `np.fft.rfft` | |
| `csnirfft` | `np.fft.irfft` | |
| `csnfft2` | `np.fft.fft2` | |
| `csnifft2` | `np.fft.ifft2` | |
| `csnrfft2` | `np.fft.rfft2` | |
| `csnirfft2` | `np.fft.irfft2` | |
| `csnfftfreq` | `np.fft.fftfreq` | |
| `csnrfftfreq` | `np.fft.rfftfreq` | |
| `csnfftshift` | `np.fft.fftshift` | |
| `csnifftshift` | `np.fft.ifftshift` | |
| `csnstft` | `scipy.signal.stft` | Frames and their coordinates. |
| `csnistft` | `scipy.signal.istft` | |

### Convolution and correlation

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csnconvolve1d` | `np.convolve` | Plus an axis: every lane along it is convolved on its own. |
| `csncorrelate1d` | `np.correlate` | Same kernel conjugation as NumPy's. |
| `csnconvolve` | `scipy.signal.convolve` | Kernel shaped like the source, flipped on every axis. |
| `csncorrelate` | `scipy.signal.correlate` | The same, unflipped and conjugated. |
| `csnfftconvolve1d` | `scipy.signal.fftconvolve` | Plus an axis. |
| `csnfftcorrelate1d` | `scipy.signal.correlate(..., method='fft')` | Plus an axis. |
| `csnfftconvolve` | `scipy.signal.fftconvolve` | N-D, kernel shaped like the source. |
| `csnfftcorrelate` | `scipy.signal.correlate(..., method='fft')` | N-D. |

The `csnfft*` four are the same operations as the four above them, computed by
padding to a power of two and multiplying in the spectrum: same arguments, same
lengths, same answers to within rounding. SciPy picks between the two methods
for you when you ask for `method='auto'`; here the choice is the opcode you
call.

The `edges` argument is NumPy's `mode` under another name: `0` FULL, `1` SAME,
`2` VALID, with the same lengths. Unlike NumPy's, it is an i-rate argument on
every overload, along with the axis, because the pair of them fixes the shape
of the output and a k-rate pass must not have to reallocate.

### Interpolation, resampling and windows

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csninterp` | `np.interp` | Linear matches; nearest / previous / next / PCHIP are `scipy.interpolate`. Boundary policy is explicit rather than a fill value. |
| `csnresample` | `scipy.signal.resample` | Interpolating resample along one axis, not Fourier. |
| `csnhanning` | `np.hanning` | |
| `csnhamming` | `np.hamming` | |
| `csnbartlett` | `np.bartlett` | |
| `csnblackman` | `np.blackman` | |
| `csnkaiser` | `np.kaiser` | |

### No NumPy counterpart

These exist because the arrays live inside a running orchestra, which is the one
thing NumPy never has to deal with:

| csnum | What it does |
| --- | --- |
| `csnfromaudio` / `csntoaudio` | One control period between an audio signal and an array. |
| `csnpack` / `csnunpack` | A whole `a[]` as a `channels x ksmps` matrix, and back. |
| `csnsnap` / `csnstream` | Frames of a size independent of `ksmps`, and their overlap-add. |
| `csnrtlock` / `csnrtunlock` | Mark a handle as a realtime path, forbidding reallocation at perf time. |
| `csnfromftable` / `csntoftable` | Csound function table in and out. |
| `csnfree` | Explicit release of a `@global` handle. |
| `csnunlikeset` | Drops the set classification. |

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

One limit is worth stating plainly, because it is easy to read the counters as
promising more than they do. A bumped data version means **a producer wrote this
slot on that pass**, not that the contents differ from the pass before. A k-rate
producer republishes its output every pass it runs, so anything downstream that
tries to answer "has a new value arrived?" from the version alone will hear yes
on every pass as soon as a single k-rate opcode sits in between. That is fine
for skipping work, which is what the counters are for — a false "changed" costs
a recomputation and nothing else. It is not enough for an opcode that must act
exactly once per arrival: `csnstream` counts hops on a phase accumulator of its
own and uses the version only to notice a producer that has stopped.

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

## Audio

Six opcodes connect the array vocabulary to Csound's audio signals. They run at
performance time, but an a-rate opcode's perf function is called once per control
period, not once per sample, so they cost what a k-rate opcode costs.

`csnfromaudio` captures one control period into an array of `ksmps` elements and
`csntoaudio` sends one back out. In between, the whole suite applies:

```csound
instr 1
    gain:k       = 0.5
    sig:a        = oscili(0.5, 440)
    block:CsnArr = csnfromaudio(sig)
    out:a        = csntoaudio(csnmul(block, gain))
endin
```

`csntoaudio` checks the element count, not the shape, so an array reshaped to a
matrix comes back out without an intervening flatten. For multichannel material
`csnpack` folds a whole `a[]` into one `channels x ksmps` array and `csnunpack`
takes it apart again — an `a[]` stores each channel as a whole `ksmps`-long
block, so the pair is a transpose of layout, not a copy of samples.

### Frames independent of ksmps

`csnsnap` slices the stream into frames of a size you choose and publishes one
every `ihop` samples, raising a ready flag on the control period where that
happens. `csnstream` overlap-adds them back:

```csound
instr 1
    sig:a               = oscili(0.5, 440)
    frame:CsnArr, new:k = csnsnap(sig, 1024, 256)
    spectrum:CsnArr     = csnrfft(frame, 1024, -1, new)
    magnitude:CsnArr    = csnabs(spectrum, new)
    out:a, ready:k      = csnstream(frame, 256)
endin
```

Here `nfft` is the 1024-sample frame size, independent of `ksmps`. `csnsnap`
marks the frame as a real-time path by default, and the FFT outputs inherit the
mark; an extra `csnrtlock` call is not required.

The hop must be at least `ksmps`: one handle names one array, so at most one
frame can be published per control period, and a smaller hop would overwrite a
frame before any consumer could read it. It is refused at init rather than
silently dropping frames. The default hop is the frame size, which gives
contiguous frames with no overlap.

`csnstream` folds in one frame per hop of output, counted on a phase accumulator
of its own, so any number of k-rate opcodes may sit between the two ends without
changing the result. With a rectangular window and `ihop` equal to the frame
length the reconstruction is exact; at 50% overlap every sample is covered twice,
so a real chain applies a window whose overlapped copies sum to one.

### Nothing allocates on the audio thread

An array published by `csnfromaudio`, `csnpack` or `csnsnap` is marked as a
realtime path, and the mark travels to everything derived from it. A marked
array refuses to reallocate during performance and names the variable that would
have done it:

```
'B' (array 4098) is on a realtime audio path and cannot be reallocated at perf
time; clear the mark with csnrtunlock, or pass irt=0 at the audio source it
descends from
```

That refusal only fires where a shape genuinely changes at k-rate. A chain whose
shapes are settled at init — the ordinary case, since `csnfromaudio` fixes its
shape at `ksmps` and every derived opcode sizes its output from its source at
init — allocates once per note and never again. Where the frames are being
harvested for analysis rather than sent back out to audio, `irt = 0` at the
source lifts the restriction for the derived arrays.

The same guarantee is available away from audio. Array work that drives a synth
at k-rate has the same intolerance for an allocation and no audio opcode to
inherit the mark from, so `csnrtlock` sets it on any handle:

```csound
src:CsnArr    = csnzeros(shape)
csnrtlock src
padded:CsnArr = csnpad(src, grow, grow, fill, trig)   ; inherits the mark
```

The i-time form affects arrays created after it and no others, so put it
immediately after the array it protects, before anything reads it. A triggered
k-rate form is also available. `csnrtunlock` clears the mark explicitly on one
handle; neither locking nor unlocking is retroactive, so arrays already derived
keep the state they inherited. In particular, `csnrtlock(handle, 0)` is an
inactive k-rate trigger, not an unlock operation.

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

## Development note

All source code in this project is written and maintained by humans.
AI tools may be used only as development assistants for code review, debugging, documentation and testing. AI suggestions are reviewed and evaluated by the project maintainers before being incorporated.
