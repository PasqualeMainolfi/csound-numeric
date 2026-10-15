# csnum: numpy-style array opcodes for Csound 7

**NOTE:** *csound-numeric is currently in the testing phase.
Bug reports, feedback, suggestions, and reports of unexpected behavior
are very welcome and greatly appreciated, as they help improve the
library and make it more stable and reliable.*

`csnum` is a Csound 7 plugin that brings a numpy-shaped array vocabulary into the
orchestra language: n-dimensional arrays with a shape and strides, elementwise
math, axis-wise reductions, slicing, sorting, statistics, linear-algebra
primitives, convolution, matrix solving, interpolation and resampling. `CsnArr`
values can also be combined with the orchestra's arithmetic operators, and read
and written with `[]`: `v[i][j]` for one element, `v[start:end]` for a slice.

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

There are **no external library dependencies** beyond Csound and the C standard
library. The plugin builds from C11 sources against the Csound plugin headers.

---

## Requirements

- **Csound 7** (API 7.0). The plugin is not built for Csound 6: the opcodes rely
  on Csound 7 user-defined types for `CsnArr` and complex arguments.
- `[]` on `CsnArr` (`v[i][j]`, `v[start:end]`) needs a Csound 7 build that lets
  a plugin type provide it: csound PR #3363, merged after 7.0.0-beta.18. Before
  it, `csnget`, `csnset` and `csngetslice` do the same work.
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
signature is covered by the i-time regression file. The real-time lock files
are scored on a final marker instead of the assertion tally: the refusals they
provoke on purpose are performance errors, which the runner counts as failed
assertions. The `.npy` tests cover file-format decoding and the i-rate/k-rate
`csnsave`/`csnload` paths, including a float32 input file.

---

## The model: `CsnArr` values, not Csound arrays

A csnum array is a value of type `:CsnArr;`: the variable owns its data, and
opcodes exchange these values, never Csound `k[]`/`i[]` arrays:

```csound
values:i[]  = fillarray(4, 1, 3, 2)
data:CsnArr = csnfromarray(values) // Csound array -> CsnArr
back:i[]    = csntoarray(data)     // CsnArr -> Csound array
```

`csnfromarray` / `csnfromftable` are the way in, `csntoarray` / `csntoftable` the
way out; on the audio path the pairs are `csnfromaudio` / `csntoaudio` and
`csnpack` / `csnunpack`. Everything in between stays in csnum arrays, so a chain
of twenty operations copies data no more than the operations themselves require.

Assignment copies: after `B = A`, writing `A` leaves `B` as it was. `csncopy(A)`
also makes an independent copy. If `A` is a valid set array, both copies retain
its set classification.

The data is released with the variable that owns it: a local at the end of its
note, an `@global` one, shared across instruments, when Csound releases its
globals.

```csound
shape@global:i[]     = fillarray(1024)
buffer@global:CsnArr = csnzeros(shape)
```

(In the orchestra header an inline `array(...)` argument is still empty when the
opcode reads it, so bind the shape to a named i-array first, as above.)

Arrays carry up to 8 dimensions and at most 2^28 elements. Shape and strides are
kept in the array, so a reshape or a transpose is a layout change, not a copy.

### Arithmetic operators

`CsnArr` supports `+`, `-`, `*`, `/` and `^` directly in Csound expressions.
They act elementwise on two arrays; `^` raises each element to the corresponding
power. Each operator also accepts a real scalar on either side, or a complex
scalar. Array shapes broadcast from the right as in NumPy, and mixing real and
complex operands produces a complex array. `*` is elementwise multiplication;
use `csnmatmul` for matrix multiplication.

```csound
instr 1
    iA[] = fillarray(2, 4, 6)
    iB[] = fillarray(1, 2, 3)
    A:CsnArr = csnfromarray(iA)
    B:CsnArr = csnfromarray(iB)

    sum:CsnArr        = A + B    // [3, 6, 9]
    difference:CsnArr = A - B    // [1, 2, 3]
    product:CsnArr    = A * B    // [2, 8, 18]
    ratio:CsnArr      = A / B    // [2, 2, 2]
    shift:CsnArr      = 10 - B   // [9, 8, 7]
    squares:CsnArr    = B ^ 2    // [1, 4, 9]

    A += B                       // [3, 6, 9]
    A -= B                       // [2, 4, 6]
    A *= 2                       // [4, 8, 12]
    A /= 2                       // [2, 4, 6]
endin
```

`+=`, `-=`, `*=` and `/=` update the left array. Assignment and the earlier
results remain independent copies. The operators take no trigger: they follow
the rates of their operands, so the i-rate example above runs during
initialization, while an operation with a k-rate operand runs on each control
period. To make an operator run at k on arrays built at init, cast one of them
with `csnk` (see [Choosing the rate](#choosing-the-rate-named-opcodes-take-a-trigger-operators-a-cast)). To inspect the result during performance, use
`csnprint(A, kTrig)` with a non-zero k-rate trigger; `csnprint(A)` reads it only
at initialization. Inside an instrument, passing `array(...)` directly to
`csnfromarray` selects its k-rate overload; use a named `i[]` built with
`fillarray` when the compound result is needed during initialization. An
operation that would overwrite values it still needs,
or change the target's layout, is refused. The corresponding named opcodes
(`csnadd`, `csnsubtract`, `csnmul`, `csndiv`, `csnpow`) remain available, including
their trigger forms.

### Indexing with `[]`

`v[i][j]` reads and writes one element, one index per dimension, and
`v[start:end, step, axis]` reads a slice. The end of a slice is included, as in
Csound's own `slicearray`, and the axis is 0 unless given.

```csound
iShape[] = fillarray(2, 3)
M:CsnArr = csnreshape(csnarange(0, 6, 1), iShape)  // [[0, 1, 2], [3, 4, 5]]

iValue = M[1][2]             // 5
M[0][0] = 10                 // [[10, 1, 2], [3, 4, 5]]
Cols:CsnArr = M[1:2, 1, 1]   // [[1, 2], [4, 5]]
```

A write with only i arguments runs once, at init; a k value or index makes it
run on every control period, and the array k-rate. `[]` needs a Csound 7 build
that lets a plugin type provide it (csound PR #3363, after 7.0.0-beta.18). See
[Indexing and slicing with `[]`](doc/indexing.md) for the rules, including
what the slice syntax cannot express.

### Choosing the rate: named opcodes take a trigger, operators a cast

**The rule is one per kind of call, and it never mixes:**

| | How the rate is chosen | i-rate | k-rate |
| --- | --- | --- | --- |
| **Named opcodes** (`csnadd`, `csnsum`, `csntranspose`, ...) | the trigger, written in the call | no trigger: `csnadd(A, B)` | a trigger: `csnadd(A, B, kTrig)` |
| **Operators** (`+`, `-`, `*`, `/`, `^`, `+=`, ...) | the rate of the operands, set with a cast | i-rate operands: `A + B` | a k-rate operand: `csnk A`, then `A + B`, or `csnk(A) + B` |

For a named opcode the trigger must be written to get the k-rate form: Csound
picks the overload at compile time from the arguments of the call, and the form
without the trigger is the init-time one, whatever the rate of the arrays you
pass it. The cast has no effect there. An operator cannot take a trigger, so it
reads the rate of its operands instead, and `csnk`/`csni` are the way to set it.

An array carries its rate: i-rate when an i-rate opcode built it, k-rate once a
k-rate opcode produces or writes it. An operator runs on every control period
only while one of its operands is k-rate, so with i-rate operands it runs once,
at init. Two casts choose the rate at the point where it matters, as `k()` and
`i()` do for scalars:

```csound
instr 1
    iShape[] = fillarray(3)
    step:CsnArr = csnfull(iShape, 0.5)
    acc:CsnArr  = csnzeros(iShape)  // i-rate: built from i arguments

    csnk acc                        // in place: acc lives at k from here on
    acc += step                     // 0.5, 1, 1.5, ... on every pass

    base:CsnArr  = csnfull(iShape, 10)
    moved:CsnArr = csnk(base) + step  // only this expression is k-rate
    first:CsnArr = csni(acc)          // acc frozen at its init value
endin
```

| You want | Write |
| --- | --- |
| an array that operators keep updating | `csnk A` before using it |
| one expression at k, the operands untouched | `csnk(A) + B` |
| the init value of a k-rate array | `csni(A)` |

Running at k does not mean recomputing on every pass: a k-rate operation skips
its work while its operands and its result are unchanged, and only an operation
written over its own operand, as `A += B`, applies on every pass. Cast before
use: an operator that met `A` still i-rate has already applied once at init.

The casts reach the operators only: `csnadd(A, B)` still runs once, at init,
even after `csnk A`, because the call without a trigger is the init-time form.
Write `csnadd(A, B, kTrig)` for k.

### Empty arrays

An array with no elements is an ordinary value here, not an error case.
`csnempty` reserves a shape without publishing any element: `csnsize` reports 0
and `csnisempty` reports 1, while `csnshape` still reports the extents that were
reserved. That reservation is the capacity `csnpush` fills, so an array built up
element by element only reallocates when it outgrows what was reserved.

```csound
cap@global:i[] = fillarray(4)
buf:CsnArr     = csnempty(cap) // size 0, room for 4
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

### String arrays: `CsnStr`

A `CsnStr` is an array of strings with the layout of a `CsnArr`: a value that
owns its strings, copied by `B = A`, released with its variable. It is a type
of its own, so the numeric opcodes refuse it when the orchestra is compiled;
the ones that also make sense on strings have a `:CsnStr;` overload, and
`csntype` reports `2` for one.

```csound
labels:S[]  = fillarray("kick", "snare", "hat", "tom")
grid:CsnStr = csnfromarray(labels)
grid[3]     = "crash"                     // [] stores a copy
first:S     = grid[0]                     // "kick"
csnsave(grid, "labels.npy")               // np.load gives dtype <U5
```

Creation (`csnempty`, `csnfull`, `csnlike`, `csnfromarray`), the way out
(`csntoarray` to `S[]`, `csnprint`), elements and slices (`csnget`, `csnset`,
`[]`, `csngetslice`, `csnsetslice`, `csntake`), the queries, the layout
opcodes (`csnreshape`, `csnflatten`, `csntranspose`, `csnflip`, `csnroll`,
`csnreverse`), `csnconcat`, `csnstack` and `csnpad` with a string fill,
`csnpush`, `csnpop`, `csninsert`, `csnremove`, `csntruncate`, `csnhead`,
`csnresize`, `csncopy`, `csnshuffle`, the masks (`csnwhere`, `csnputmask`,
`csncompress`, `csnselect`), the elementwise `csnstradd`, `csnstrmul`,
`csnstrupper`, `csnstrlower`, `csnstrcapitalize` and `csnstrswapcase`, the
rate casts `csni` and `csnk`, and `csnsave` / `csnload` (`.npy` as `<U` or
`|S`, `.csn`) take one today. A string has no k form, so their k forms take an
explicit trigger. Each element keeps the buffer of its string, so on a
[real-time path](#real-time-paths) a k pass writes into the room the init pass
gave it and stops only at a string that does not fit. See
[String arrays](doc/string-arrays.md) for the list, the file formats, the
real-time rule and a runnable example.

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
`csnprint(data, trig)`, prints on a non-zero trigger and emits nothing when the
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
  function tables, copy, type and shape queries, and numpy-style printing.
- **Shape and layout**: reshape, flatten, transpose, flip, roll, pad, truncate,
  head, resize, concat, insert, remove, push, pop.
- **Indexing**: element get/set, also as `v[i][j]`, slices, also as
  `v[start:end, step, axis]`, gathers, integer-bin counting
  (`bincount`), insertion-point search (`searchsorted`), and the
  index-returning searches (`indexof`, `argwhere`, `argnonzero`, `argisnan`).
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
  projection and rejection, reflection, the three matrix operations that need
  a factorization — `csnsolve`, `csninv`, `csndet` — and `csnsavgol`, which
  builds a Savitzky-Golay filter for `csncorrelate1d` to apply.
- **Complex**: real / imaginary parts, angle, conjugate, conversion to and from
  real arrays, and scalar arithmetic on a `:Complex;` value — `csnadd`,
  `csnsubtract`, `csnmul`, `csndiv`, `csnpow`, `csnsqrt` with two complex
  operands or one complex and one real, plus `csnabs`, `csnangle` and `csnconj`
  on a single one.
- **Fourier analysis**: full and real FFT/IFFT along one axis or across a 2-D
  matrix, STFT/ISTFT, frequency-coordinate arrays, FFT shift/unshift, and the
  DCT-I/II and DST-I/II at any length, the analytic signal in one and two
  dimensions and the Hilbert transform on its own.
- **Cepstral and mel analysis**: mel-frequency cepstral coefficients in one
  opcode, and the triangular mel filterbank itself, linear or logarithmic.
- **Convolution and correlation**: convolution and cross-correlation, with a
  1-D kernel flat or along one axis, or with a kernel shaped like the source,
  over NumPy's FULL / SAME / VALID spans — computed directly, or through
  Fourier transforms for the kernels long enough to make that pay.
- **Interpolation and resampling**: `csninterp` (linear, nearest, previous,
  next, monotone cubic PCHIP, with error / clamp / fill / extrapolate boundary
  policies), `csnresample`, `csnlagrange` for the coefficients of the
  polynomial through a set of points, and `csnroots` for the roots of a
  polynomial, one per slice along an axis.
- **Filter design**: digital Butterworth, Chebyshev type I and II and
  elliptic filters as sections or coefficients (`csnbuttersos`,
  `csncheby1sos`, `csncheby2sos`, `csnellipsos` and their `ba` forms), applied
  to arrays or audio with `csnsosfilter` and `csnlfilter`, or with an explicit
  state through `csnzsosfilter` and `csnzlfilter`, the same four families on
  audio with a k-rate cutoff or band, redesigned every control period without
  allocating (`csnrtbutter`, `csnrtcheby1`, `csnrtcheby2`, `csnrtellip`), the analog
  Butterworth prototype (`csnbuttap`), the transforms of a lowpass prototype to lowpass,
  highpass, bandpass and bandstop, on transfer-function coefficients or on
  zeros, poles and gain, the bilinear transform to a
  digital filter, and `csntftozpk` / `csnzpktotf` between transfer-function
  coefficients and zeros, poles and gain, and `csnzpktosos` into second-order
  sections.
- **Windows**: Hann, Hamming, Bartlett, Blackman, Kaiser.
- **Persistence**: `csnsave` and `csnload` use `.csn` for csnum's own format or
  `.npy` for NumPy interoperability, preserving shape and real/complex type.
- **Coordinate systems**: conversions between cartesian coordinates and the
  polar, cylindrical and spherical systems, each pair named for its own; the
  spherical order is `(r, theta, phi)` with `theta` the inclination from `+z`,
  and an optional flag takes or answers the angles in degrees. `csndegtorad` and
  `csnradtodeg` also convert a single number.
- **Special functions**: the associated Legendre functions and the real
  spherical harmonics of the AmbiX convention (ACN, SN3D), one at a time or the
  whole set up to an order, at one direction or as a channels x directions
  matrix to build a decoder from. Neither carries the Condon-Shortley phase.
- **Room acoustics**: reverberation time by Sabine and by Eyring-Norris from a
  volume and its absorbing surfaces, the absorption a target time demands, the
  Schroeder frequency that separates a room's modal region from its diffuse one,
  and the modal frequencies of a rectangular room. Either operand may be an
  array, so a set of rooms or a range of targets is one call. `csnt60tofbg` and
  `csnfbgtot60` cross from the room to the reverb that models it: the feedback
  gain a comb of a given delay needs to decay by 60 dB in a given time, and the
  decay time a gain actually produces.
- **Levels and time conversion**: `csndbsum` adds levels in dB by adding their
  powers — over a pair, a whole array, or one axis — and the four sample/time
  conversions take the sample rate as an argument rather than reading the
  orchestra's, so material carrying a rate of its own converts against it.
- **Audio bridge**: `csnfromaudio` / `csntoaudio` move one control period between
  an audio signal and an array, `csnpack` / `csnunpack` do the same for a whole
  `a[]` as a `channels x ksmps` matrix, and `csnsnap` / `csnstream` slice a stream
  into overlapping frames of a size independent of `ksmps` and overlap-add them
  back. Arrays on an audio path refuse to reallocate during performance.

### Conventions shared by the whole suite

- **Axis argument.** Explicit axes follow NumPy indexing: `0` is the first axis,
  `-1` the last, `-2` the penultimate, and valid values run from `-ndim` to
  `ndim - 1`. Omitting the axis is a separate operation, never an alias for
  passing `-1`: depending on the opcode it means flatten first, operate on all
  axes, use the last axis, or is not allowed. Each opcode documents its default.
  The broad families are: flat for global reductions, cumulative operations,
  normalisation, moving statistics, interpolation/resampling and 1-D
  convolution; all axes for flip, pad and truncate; last axis for FFT-like
  transforms, difference and sorting. Structural operations such as take,
  slice and stack require an axis; the slice syntax `v[start:end]` is the
  exception, along axis 0 unless an axis follows the step.
- **In-place forms.** Where it makes sense, the opcode that produces a new
  array also has a sibling under the same name that writes back into its source
  and returns nothing: `csnnormalize(data)` normalizes in place, while
  `Norm:CsnArr = csnnormalize(data)` leaves the source alone.
- **Choosing the rate.** Named opcodes are chosen by their trigger, which must
  be written to get the k-rate form: without it the call is the init-time form,
  whatever the rate of its operands. The operators take no trigger and follow
  the rate of their operands, which `csnk` and `csni` set; see
  [Choosing the rate](#choosing-the-rate-named-opcodes-take-a-trigger-operators-a-cast).
  The i-rate and k-rate forms of a named opcode share its name; Csound picks the
  overload from the arguments you pass.
- **Trigger.** Most k-rate forms take a trigger. When an axis may be omitted,
  the no-axis overload ends with the trigger and the explicit-axis overload is
  written `..., trigger, axis`; this keeps omission distinct from `axis = -1`.
  A zero trigger skips the pass entirely and keeps the previous result. Where
  the trigger is the only k-rate argument, as in `csnprint`, it is required so
  Csound can distinguish the performance overload from the init-time one.
  `csnprint`, `csnsave` and `csnload` have no previous computed result to
  keep, so a zero trigger simply performs no side effect.
- **Real-time paths.** An array on a live signal chain can be marked so that
  nothing reallocates it, or any buffer serving it, during performance. The
  audio sources mark what they publish by default (`irt = 1`); `csnrtlock` marks
  one array, `csnrtlockstart` / `csnrtlockend` everything a note creates
  between the two, and `csnrtlockall` everything for the whole performance.
  [Real-time paths](#real-time-paths) explains how the mark works and why it
  matters.

---

## NumPy correspondence

The names follow NumPy wherever the operation is the same one. The table below
maps every opcode to the NumPy call it stands for, family by family, so a
function you already know can be found under its Csound name. The mapping is by
*intent*, not by signature: csnum opcodes exchange `CsnArr` values, take their shape
arguments as Csound i-arrays, and have no keyword arguments, so the NumPy column
is what the opcode computes, not a transliteration of its call.

Four differences apply throughout and are not repeated in every row:

- **Axis.** As in NumPy, a supplied negative axis counts from the end, so `-1`
  always means the last axis. Axis omission is represented by a separate
  overload and may mean flat, all axes, or last axis according to the operation.
- **Booleans.** csnum has no boolean element type. The comparisons, the `is*`
  predicates and the logical operations return a real array of `0` and `1` where
  NumPy returns `bool_`, and any of them is accepted as a mask.
- **Index results.** `csnindexof`, `csnargmin`, `csnargmax`, `csnargsort`,
  `csnargwhere` and friends return **coordinates**, one per source dimension,
  where NumPy often returns flat indices unless you call `np.unravel_index`.
- **Broadcasting.** Elementwise binary arithmetic, comparisons and logical
  operations broadcast array shapes from the right, as in NumPy; an extent of
  `1` can expand to its partner's extent. `csnmatmul` broadcasts batch axes.
  Operations with different semantics, such as `csnstack`, `csnpairdist` and
  `csnwhere`, still have their own exact-shape requirements.

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
| `csntype` | `ndarray.dtype` | `0` real, `1` complex, `2` string. |
| `csndims` | `ndarray.ndim` | |
| `csnsize` | `ndarray.size` | |
| `csnshape` | `ndarray.shape` | |
| `csnisempty` | `a.size == 0` | |
| `csnprint` | `print(a)` | Same bracket and indentation layout, same 1000-element summarization. |

### Persistence

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csnsave` | `np.save` | A `.npy` path writes a NumPy file (`float64`, `complex128`, or `<U` for a `CsnStr`); `.csn` writes csnum's own format. |
| `csnload` | `np.load` | A `.npy` path reads supported numeric dtypes, or `<U` / `\|S` into a `CsnStr`, in C or Fortran order; `.csn` reads csnum's own format. |
| `csnloadaudiofile` | `librosa.load(path, sr=None, mono=False)` | Reads `.wav`, `.mp3` or `.flac`, one row per channel, and returns the file's sample rate; never resamples. |
| `csnsaveaudiofile` | `soundfile.write` | Writes `.wav` as 32-bit float, one channel per row. |

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
| `a[i][j]` | `a[i, j]` | One index per dimension; also `a[i][j] = v`. |
| `a[start:end, step, axis]` | `np.take(a, range(start, end + 1, step), axis)` | End included, as Csound's `slicearray`; axis 0 by default. |
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
| `csnindexof` | `np.argwhere(a == value)[0]` | First matching coordinate; an empty array when no element matches. |
| `csnbincount` | `np.bincount` | 1-D non-negative integer source; optional weights, but no `minlength`. |
| `csnsearchsorted` | `np.searchsorted` | Ascending 1-D real source; `side` is `0` for left and `1` for right; no `sorter`. |
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
| `csndivmod` | `np.divmod` | Two arrays out. |
| `csnhypot` | `np.hypot` | |
| `csnminimum` | `np.minimum` | |
| `csnmaximum` | `np.maximum` | |
| `csnclip` | `np.clip` | |
| `csnabs` | `np.abs` | Magnitude for complex. |
| `csnsign` | `np.sign` | |
| `csnfloor` | `np.floor` | |
| `csnceil` | `np.ceil` | |
| `csnround` | `np.round` | Halves go to the even neighbour, as in NumPy and Csound's scalar `round`; Csound's `round` over an array sends them away from zero. |

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
| `csnmovmean` | — | `pandas.Series.rolling(w, center=True, min_periods=1).mean()`: the window is centred and shorter near the ends, and every moving statistic below uses it. `np.convolve` with a box agrees away from the edges. |
| `csnmovmedian` | — | `rolling(...).median()`; a NaN in the window gives NaN, as `np.median` does. |
| `csnmedfilt1d` | — | `scipy.signal.medfilt` along one axis: the same centred window, padded with zeros instead of shortened at the ends. |
| `csnmedfilt` | — | `scipy.signal.medfilt` over a box of one size per axis, `scipy.signal.medfilt2d` for a 2-D pair. |
| `csnmovmin` | — | `rolling(...).min()`. |
| `csnmovmax` | — | `rolling(...).max()`. |
| `csnmovstd` | — | `rolling(...).std(ddof=0)`, the population deviation; pandas defaults to `ddof=1`. |
| `csnmovvar` | — | `rolling(...).var(ddof=0)`. |

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
| `csnsolve` | `np.linalg.solve` | One factorization, any number of right-hand sides. |
| `csninv` | `np.linalg.inv` | |
| `csndet` | `np.linalg.det` | Zero for a singular matrix, as NumPy's does; `csninv` and `csnsolve` refuse one. |
| `csnhilbertmat` | `scipy.linalg.hilbert` | The matrix, not a transform. |
| `csnsavgol` | `scipy.signal.savgol_coeffs` | Every derivative order at once, one per row; apply it with `csncorrelate1d`. |

### Complex arrays

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csnreal` | `np.real` | |
| `csnimag` | `np.imag` | |
| `csnangle` | `np.angle` | |
| `csnconj` | `np.conj` | |
| `csntocomplex` | `a.astype(complex)` | |
| `csntoreal` | `a.real` | Keeps the real parts. |

### String arrays

A `CsnStr` stands for a NumPy array of dtype `str` (`<U`). These are the
overloads that take one; the numeric opcodes refuse it at compile time.

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csnempty` | `np.empty(shape, dtype=str)` | Reserved and empty, as for numbers. |
| `csnfull` | `np.full(shape, "s")` | |
| `csnlike` | `np.full_like(a, "s")` | The k form takes a trigger. |
| `csnfromarray` | `np.array(list_of_str)` | From an `S[]`; the k form takes a trigger. |
| `csntoarray` | `ndarray.tolist` | To an `S[]`; the k form takes a trigger. |
| `csnget` / `csnset` | `a[i, j]` / `a[i, j] = "s"` | Also `v[i][j]`. |
| `csngetslice` | `a[start:stop:step]` along an axis | Also `v[start:end]`; the slice owns its strings. |
| `csnsetslice` | `a[start:stop:step] = b` along an axis | Each slot takes a copy; an unchanged slot allocates nothing. |
| `csntake` | `np.take(a, i, axis)` / `a.flat[i]` | Along an axis to a `CsnStr`, flat to an `S`. |
| `csnsize`, `csndims`, `csnshape`, `csnisempty`, `csntype` | `a.size`, `a.ndim`, `a.shape`, `a.size == 0`, `a.dtype` | `csntype` is `2`. |
| `csnreshape`, `csnflatten` | `np.reshape`, `np.ravel` | In place: `a.shape = ...`, `a.shape = (a.size,)`. |
| `csntranspose`, `csnflip`, `csnroll`, `csnreverse` | `np.transpose`, `np.flip`, `np.roll`, `a[::-1]` | A copy owns its strings; in place nothing is allocated. |
| `csnconcat` | `np.concatenate` | Two `CsnStr`; flat without an axis. |
| `csnstack` | `np.stack` | Two or more `CsnStr`, all of the same shape. |
| `csnpad` | `np.pad(a, w, constant_values="s")` | The fill string is required; the k form in place is refused on a real-time path. |
| `csnpush`, `csninsert` | `np.append`, `np.insert` | In place, one `S` or a `CsnStr` block; the block k form is refused on a real-time path. |
| `csnpop`, `csnremove` | `a[-1]` + `np.delete`, `np.delete` | One element comes back as an `S`, allowed on a real-time path; a removed block is a new `CsnStr`. |
| `csntruncate`, `csnhead` | `a[:n]` | In place the strings are only moved, so the k form is allowed on a real-time path. |
| `csnresize` | `ndarray.resize` | What it adds is `""`. |
| `csncopy`, `csni`, `csnk` | `np.copy` | Each copy owns its strings. |
| `csnshuffle` | `np.random.shuffle` | The same permutation as a numeric array after the same `csnseed`; allowed on a real-time path. |
| `csnwhere` | `np.where(mask, a, b)` | Real mask; `b` a `CsnStr` or one `S`. |
| `csnputmask` | `np.putmask(dest, mask, values)` | Destination first, unlike the numeric form; unchanged elements are not rewritten. |
| `csncompress`, `csnselect` | `np.compress`, `a[mask]` | Real mask; the kept strings are copies. |
| `csnstradd` | `np.strings.add` | Two `CsnStr` with broadcasting, or one `S` before or after every element. |
| `csnstrmul` | `np.strings.multiply` | One count or a real `CsnArr` of counts; whole numbers, below 0 gives `""`. |
| `csnstrupper`, `csnstrlower` | `np.strings.upper`, `np.strings.lower` | ASCII letters only, as on a bytes (`S`) array: bytes from 0x80 up stay, so UTF-8 survives. Also in place. |
| `csnstrcapitalize` | `np.strings.capitalize` | First character up, the rest down; ASCII only, as on a bytes (`S`) array. Also in place. |
| `csnstrswapcase` | `np.strings.swapcase` | ASCII letters only, as on a bytes (`S`) array. Also in place; `X = csnstrswapcase(X, k)` swaps again on every pass. |
| `csnprint` | `print(a)` | `CsnStr(shape=..., dtype=str)`, elements quoted. |
| `csnsave` / `csnload` | `np.save` / `np.load` | `.npy` as `<U` (UTF-8 decoded to code points), loads `<U` and `\|S`; `.csn` dtype `3`. |

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
| `csndctone1d` | `scipy.fft.dct(x, type=1)` | `norm=None`. Any length. |
| `csndcttwo1d` | `scipy.fft.dct(x, type=2)` | `norm=None`. Any length. |
| `csndstone1d` | `scipy.fft.dst(x, type=1)` | `norm=None`. Any length. |
| `csndsttwo1d` | `scipy.fft.dst(x, type=2)` | `norm=None`. Any length. |
| `csnhilbert1d` | `scipy.signal.hilbert` | The analytic signal, as scipy returns. Even length. |
| `csnhilbert1dr` | `np.imag(scipy.signal.hilbert(x))` | The transform alone, computed with two real FFTs. |
| `csnhilbert2` | `scipy.signal.hilbert2` | |

### Cepstral and mel analysis

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csnmfcc` | `librosa.feature.mfcc` | HTK mel scale, Slaney-normalized bands, natural log. One argument sets both band and coefficient count. |
| `csnmfbank` | `librosa.filters.mel` | The matrix itself, `(bands, nfft / 2 + 1)`. |
| `csnmlogfbank` | — | The same matrix in logarithmic scale. |

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
| `csndeconvolve1d` | `scipy.signal.deconvolve` | The quotient, plus an axis. Undoes a FULL `csnconvolve1d`. |
| `csndecorrelate1d` | `scipy.signal.deconvolve(c, np.conj(h[::-1]))` | Undoes a FULL `csncorrelate1d`. Plus an axis. |
| `csndeconvolve` | — | N-D, kernel shaped like the source. |
| `csndecorrelate` | — | N-D. |
| `csnfftdeconvolve1d` | `np.fft.ifft(np.fft.fft(x, n) / np.fft.fft(h, n))[:len(x) - len(h) + 1]` | Plus an axis; `n` the next power of two at or above `len(x)`. |
| `csnfftdecorrelate1d` | the same with `np.conj(np.fft.fft(h, n))`, read from `len(h) - 1` | Plus an axis. |
| `csnfftdeconvolve` | the `fftn` form of the same | N-D. |
| `csnfftdecorrelate` | the `fftn` form of the same | N-D. |

The `csnfft*` four are the same operations as the four above them, computed by
padding to a power of two and multiplying in the spectrum: same arguments, same
lengths, same answers to within rounding. SciPy picks between the two methods
for you when you ask for `method='auto'`; here the choice is the opcode you
call.

The `edges` argument is NumPy's `mode` under another name: `0` FULL, `1` SAME,
`2` VALID, with the same lengths. Unlike NumPy's, it is an i-rate argument on
every overload, along with the axis, because the pair of them fixes the shape
of the output and a k-rate pass must not have to reallocate.

The deconvolve and decorrelate forms have no `edges` argument: they undo the
FULL span, the only one that keeps everything the source contributed, and
answer `x - h + 1` on each axis. The direct forms run the convolution sum
backwards, dividing by `h[0]` (deconvolution) or `conj(h[last])`
(decorrelation), which must be non-zero, and are stable only on a minimum-phase
kernel (reversed, for a decorrelation). The `csnfft*` forms divide spectra, have
no phase condition, and refuse a kernel with a spectral zero on the transform
grid, such as `[1, 1]` at Nyquist. The two routes agree when the input really is
a FULL convolution or correlation; otherwise the direct form returns the
quotient of a polynomial division and the transform a circular one.

### Interpolation, resampling and windows

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csninterp` | `np.interp` | Linear matches; nearest / previous / next / PCHIP are `scipy.interpolate`. Boundary policy is explicit rather than a fill value. |
| `csnresample` | `scipy.signal.resample` | Interpolating resample along one axis, not Fourier. |
| `csnlagrange` | `scipy.interpolate.lagrange` | The coefficients, highest degree first, as `np.poly1d` takes them. Distinct `x` only. |
| `csnroots` | `np.roots` | Same coefficient order. Always complex, in no set order; one polynomial per slice along an optional axis. A leading zero is refused, not stripped. |
| `csnhanning` | `np.hanning` | |
| `csnhamming` | `np.hamming` | |
| `csnbartlett` | `np.bartlett` | |
| `csnblackman` | `np.blackman` | |
| `csnkaiser` | `np.kaiser` | |

### Filter design

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csnlptolpzpk` | `scipy.signal.lp2lp_zpk` | |
| `csnlptohpzpk` | `scipy.signal.lp2hp_zpk` | A root at the origin is refused. |
| `csnlptobpzpk` | `scipy.signal.lp2bp_zpk` | The two roots of each pair are stored next to each other, not in two halves. |
| `csnlptobszpk` | `scipy.signal.lp2bs_zpk` | Same pair order; a root at the origin is refused. |
| `csnbilinearzpk` | `scipy.signal.bilinear_zpk` | `fs` in Hz, greater than zero. |
| `csntftozpk` | `scipy.signal.tf2zpk` | Real coefficients only. Roots in no set order. |
| `csnzpktotf` | `scipy.signal.zpk2tf` | 1-D zeros only. Real coefficients when the roots come in conjugate pairs, up to `1e-12` of the largest. |
| `csnlptolp` | `scipy.signal.lp2lp` | |
| `csnlptohp` | `scipy.signal.lp2hp` | |
| `csnlptobp` | `scipy.signal.lp2bp` | |
| `csnlptobs` | `scipy.signal.lp2bs` | |
| `csnzpktosos` | `scipy.signal.zpk2sos` | Digital, `pairing='nearest'` only. |
| `csnbuttap` | `scipy.signal.buttap` | |
| `csnbutterba` | `scipy.signal.butter` | Digital, `output='ba'`, frequencies in Hz with `fs` given; type `0` low, `1` high, `2` band, `3` stop. |
| `csnbuttersos` | `scipy.signal.butter` | The same with `output='sos'`; the form to use beyond the lowest orders. |
| `csncheby1ba` | `scipy.signal.cheby1` | `output='ba'`; `rp` after the cutoff, not before it. |
| `csncheby1sos` | `scipy.signal.cheby1` | `output='sos'`; `rp` after the cutoff, not before it. |
| `csncheby2ba` | `scipy.signal.cheby2` | `output='ba'`; `rs` after the cutoff, not before it. |
| `csncheby2sos` | `scipy.signal.cheby2` | `output='sos'`; `rs` after the cutoff, not before it. |
| `csnellipba` | `scipy.signal.ellip` | `output='ba'`; `rp`, `rs` after the cutoff, not before it. |
| `csnellipsos` | `scipy.signal.ellip` | `output='sos'`; `rp`, `rs` after the cutoff, not before it. |
| `csnlfilter` | `scipy.signal.lfilter` | Transposed direct form II along an axis; k-rate keeps each slice's state between passes, as `zi = zf`; audio form too. |
| `csnsosfilter` | `scipy.signal.sosfilt` | The same with second-order sections; `sos[:, 3]` must be 1. |
| `csnzlfilter` | `scipy.signal.lfilter(..., zi=zi)` | Returns `(y, zf)`; `zi` in scipy's layout, the source's shape with the axis replaced by the order. |
| `csnzsosfilter` | `scipy.signal.sosfilt(..., zi=zi)` | Returns `(y, zf)`; `zi` is `(n_sections, ...)` with the axis replaced by 2. |
| `csnrtbutter` | `scipy.signal.butter(..., output='sos')` + `sosfilt` | Audio only, redesigned every control period from a k-rate cutoff, the state kept; a band is centre and width in Hz, `[fc - bw/2, fc + bw/2]`. |
| `csnrtcheby1` | `scipy.signal.cheby1(..., output='sos')` + `sosfilt` | The same; `rp` after the cutoff (and width). |
| `csnrtcheby2` | `scipy.signal.cheby2(..., output='sos')` + `sosfilt` | The same; `rs` after the cutoff (and width). |
| `csnrtellip` | `scipy.signal.ellip(..., output='sos')` + `sosfilt` | The same; `rp`, `rs` after the cutoff (and width). |

### Special functions

| csnum | NumPy | Notes |
| --- | --- | --- |
| `csnlegendre` | `scipy.special.lpmv` | Arguments `(n, m, x)`, not `(m, n, x)`, and no Condon-Shortley phase: `lpmv(m, n, x) == (-1)^m * csnlegendre(n, m, x)`. `m >= 0` only. |
| `csnsphharm` | — | The real AmbiX harmonic, SN3D, no phase, no `1/sqrt(4 pi)`. `scipy.special.sph_harm` is the complex, orthonormal one with the phase: for `m != 0` this one is the real or imaginary part of that, renormalised, not a rescaling of the same value. |
| `csnsphharmacn` | — | Every harmonic up to an order, by ACN, at one direction or as a `channels x directions` matrix. |

### No NumPy counterpart

These exist because the arrays live inside a running orchestra, which is the one
thing NumPy never has to deal with:

| csnum | What it does |
| --- | --- |
| `csnfromaudio` / `csntoaudio` | One control period between an audio signal and an array. |
| `csnk` / `csni` | Rate casts: an array made k-rate in place or as a k-rate copy, or frozen at its init value. NumPy has no rates. |
| `csnpack` / `csnunpack` | A whole `a[]` as a `channels x ksmps` matrix, and back. |
| `csnsnap` / `csnstream` | Frames of a size independent of `ksmps`, and their overlap-add. |
| `csnrtlock` / `csnrtunlock` | Mark an array as a real-time path, forbidding reallocation at perf time. |
| `csnrtlockstart` / `csnrtlockend` | The same mark on every array a note creates between the two. |
| `csnrtlockall` | The same for the whole performance, declared in the orchestra header. |
| `csnforeach` | Maps a Csound opcode over every element, in place. Nearest idea is `np.vectorize`, but the callback is an orchestra opcode. |
| `csnnext` | Walks an array one element per control period. An iterator paced by the control rate, which NumPy has no notion of. |
| `csnt60sab` / `csnt60eyr` | Reverberation time of a room, Sabine and Eyring-Norris. |
| `csnrt60absp` | The absorption a room needs to reach a target reverberation time. |
| `csnfschrd` | Schroeder frequency: where a room stops being modal and becomes diffuse. |
| `csnfpqr` | Resonant frequency of a rectangular room for one modal index triple. |
| `csnt60tofbg` / `csnfbgtot60` | Comb feedback gain from a decay time, and the decay time back. |
| `csndbsum` | Adds levels in dB by adding their powers, over a pair, an array or one axis. |
| `csnsamptomillis` / `csnsamptosec` | Sample counts into milliseconds or seconds at a given rate. |
| `csnmillistosamp` / `csnsectosamp` | Milliseconds or seconds into sample counts at a given rate. |
| `csnpoltocar` / `csncartopol` | Polar and cartesian coordinates, in the plane. |
| `csncyltocar` / `csncartocyl` | Cylindrical and cartesian coordinates. |
| `csnsphtocar` / `csncartosph` | Spherical and cartesian coordinates, `(r, theta, phi)`. |
| `csnhoatocar` / `csncartohoa` | Ambisonics direction and cartesian, `(distance, azimuth, elevation)` in the AmbiX frame. The elevation is measured from the horizon, not from `+z`. |
| `csnrotmat` | A 3 x 3 rotation matrix about a basis axis or an arbitrary direction, for `csnmatmul`. |
| `csnrotmatypr` | The same, composing a yaw, a pitch and a roll in the audio convention, in the one order that is meant. |
| `csnnmtoacn` / `csnacntonm` | Ambisonics channel index from an order and a degree, and back. ACN ordering. |
| `csnhoaordtochnls` / `csnchnlstohoaord` | Channels in a complete 3-D set of a given order, and back. A count that is not a perfect square is refused. |
| `csnsn3dton3d` / `csnn3dtosn3d` | The per-channel gain vector between the SN3D and N3D normalisations, indexed by ACN. |
| `csnfromftable` / `csntoftable` | Csound function table in and out. |
| `csnunlikeset` | Drops the set classification. |

---

## k-rate performance: array versioning

The interesting part of csnum is what happens when these opcodes run inside the
k-rate loop, thousands of times per second, on arrays that usually have not
changed since the last pass.

Every array carries four counters, data, shape, ndim and item type, bumped only
by a writer that actually changed that aspect. A k-rate opcode records the
version of its source, and of the output it writes, at the end of a pass that
did real work. On the next pass it compares:

- the **source** version, including its array identity, so a different array
  with the same counters is not mistaken for the previous input;
- the **output** version, so an opcode that finds its result untouched
  can reuse it instead of recomputing it;
- the **request**, shape, ndim, item type and any scalar parameter it depends
  on, such as an axis or a window size.

If all of them match, the opcode keeps last pass's result and returns. No
allocation, no copy, no arithmetic. A chain of csnum opcodes driven by a source
that moves once every few hundred k-cycles therefore costs almost nothing on the
passes in between, and the saving compounds along the chain: a consumer sees its
own input as unchanged precisely because the producer upstream skipped its work.

Three details make this safe rather than merely fast:

- **The output array is reused.** A k-rate producer writes into its
  destination variable for the life of the note. The array is created with room for
  twice its initial element count, and a later shape that fits that room
  reuses it; new storage is taken only when the requested shape no longer
  fits.
- **In-place opcodes publish their write.** They bump the data version *and*
  record it as their own, so the next pass recognizes its own handiwork and
  leaves it alone, while every other consumer still sees a new generation.
- **Self-aliasing is checked per operation.** Elementwise operations can read
  and write the same cell in place when shape and type stay the same. Layout
  changes and operations with their own safe in-place algorithm also allow it.
  Operations that would overwrite input they still need refuse the alias.

Scratch buffers are per-opcode-instance and reserved at init from the capacity
of the array they serve, so a k-rate pass allocates nothing in steady state.
Off a real-time path they may still grow, geometrically, when an array outgrows
that capacity; on one they may not (see [Real-time paths](#real-time-paths)).

One limit is worth stating plainly, because it is easy to read the counters as
promising more than they do. A bumped data version means **a producer wrote this
array on that pass**, not that the contents differ from the pass before. A k-rate
producer may rewrite its output on every pass it runs, so anything downstream that
tries to answer "has a new value arrived?" from the version alone will hear yes
on every pass as soon as a single k-rate opcode sits in between. That is fine
for skipping work, which is what the counters are for — a false "changed" costs
a recomputation and nothing else. It is not enough for an opcode that must act
exactly once per arrival: `csnstream` counts hops on a phase accumulator of its
own and uses the version only to notice a producer that has stopped.

### What the init pass computes

A k-rate opcode also runs an init pass, and it computes its result there only
when every argument the result depends on is known at init: a constant, an i
variable or an array. A k scalar is not: an ordinary k variable still reads 0
during the init pass, so a result computed from it would be wrong, and nothing
downstream could tell. With such an argument the init only lays the output out
and the first triggered pass computes it:

- an opcode that keeps the rank (`csngetslice`, `csnpad`, `csnconcat`,
  `csntranspose`, `csnreshape`) gives the source's layout, reserved but empty;
- one whose shape the k argument decides (a reduction or `csndiff` along a k
  axis, `csnmedfilt1d`) gives an empty 1-D array;
- an elementwise one (`csnclip`, `csngt` and the comparisons) keeps the
  source's shape with values not computed yet; `csnrotmat` gives a zero matrix.

```csound
instr 1
    trig:k        = metro(10)
    axis:k        = 0
    fixed:CsnArr  = csnsum(mat, trig, 0)    // reduced at init and on each trigger
    moving:CsnArr = csnsum(mat, trig, axis) // empty until the first trigger
endin
```

An output written over its own operand (`X = csnclip(X, lo, hi, trig)`) is not
computed at init either: every pass applies it, so the first one does. A
consumer that cannot take an empty array belongs behind the same trigger; the
k-rate `csntoarray` accepts one at init and converts from the first pass.

---

## Real-time paths

### Why perf time is different

Csound runs every opcode of every active note once per control period, on the
same thread that fills the audio buffer. In real time that thread works against a
deadline: the next buffer is due whether or not the orchestra has finished
computing it. At 48 kHz with `ksmps = 32` a control period lasts two thirds of a
millisecond.

A call into the allocator has no bounded running time. `malloc` and `realloc`
can take a lock another thread holds, walk long free lists, or ask the operating
system for fresh pages. Most calls return quickly, but it only takes one slow
call to miss the deadline, and a missed deadline is a dropout: a click in the
output, intermittent, and hard to trace back to its cause. That is why
real-time audio code allocates what it needs up front and never on the audio
thread.

csnum arrays are dynamic by design. Most k-rate opcodes can change the shape of
what they publish from one pass to the next, and a larger shape needs new
storage. Offline, or on a chain that only feeds analysis, that is harmless. On a
chain that ends in the speakers it is exactly the kind of allocation real-time
code must not do. The real-time mark is how you tell csnum which arrays are on
such a chain.

### What the mark guarantees

A marked array, and every buffer an opcode keeps to produce or rewrite it,
never takes new storage during performance. Init time is unrestricted, and it
is where the storage is taken. Writing new values, and changing the layout
within the storage already there, stay allowed at any time, so a marked array
can be reshaped, truncated, or resized back up to its capacity.

When a pass would need more storage than a marked array has, the opcode does
not allocate. It raises a performance error that names the array, which stops
the note:

```
PERF ERROR in instr 1 (opcode csnpad) line 36: [csnarray] Array is on a
real-time path and cannot grow from 64 to 96 elements at perf time; clear the
mark with csnrtunlock, or pass irt=0 at the audio source it descends from
```

That error is the point of the mark. A performance error stops the note the
moment the chain does something it cannot afford, and it tells you which array
did it. A dropout gives no such report.

### How arrays get marked

Every way of marking sets the same flag on an array:

| Source of the mark | When it applies | What it covers |
| --- | --- | --- |
| `csnfromaudio`, `csnpack`, `csnsnap` (`irt = 1`, the default) | when the array is published | the audio frame and everything derived from it |
| `csnrtlock data` | at init, or on a triggered k-rate pass | that one array |
| `csnrtlockstart` ... `csnrtlockend` | while the section is open | every array the current note creates in between; the note ending also ends the section |
| `csnrtlockall` | from its line in the orchestra header onward | every array created for the rest of the performance, with no way to switch it off |

An array derived from a marked operand inherits the mark when it is created, so
marking the source of a chain marks the chain. The mark belongs to the array it
is set on:

- locking a source later does not reach the arrays already derived from it;
- `csnrtunlock` clears one array and nothing else, so a branch that only feeds
  analysis can be released while the rest of the chain stays marked;
- `csnrtlock(data, 0)` is an inactive k-rate trigger, not an unlock.

```csound
instr 1
    trig:k          = 1
    win:k           = 5
    src:CsnArr      = csnzeros(fillarray(64))
    csnrtlock src
    smooth:CsnArr   = csnmovmedian(src, win, -1, trig)  ; inherits the mark

    csnrtlockstart
    frame:CsnArr    = csnzeros(fillarray(256))          ; marked by the section
    spectrum:CsnArr = csnrfft(frame, 256, -1, trig)     ; marked by the section
    csnrtlockend
endin
```

### What it covers

The mark is enforced wherever storage could grow during performance, not only
where an opcode publishes a result:

- **Published outputs.** An output keeps the storage it was created with, room
  for twice its initial element count, and reuses it for any later shape that
  fits. A marked output can therefore shrink, grow back, or change its count
  with the data (`csncompress`, `csnselect`, `csnunique`, the set operations)
  without new storage; only a shape that needs more than that room is
  refused.
- **Arrays rewritten in place.** `csnpush`, `csninsert`, `csnsetinsert`, and
  `csnpad` or `csnresize` without an output refuse to grow a marked array past
  its capacity.
- **Working buffers.** The scratch an opcode keeps for itself, the sort buffer
  of a median or the sorted window of `csnmovmedian` for instance, is reserved
  at init for the most the array it serves can hold. A window that grows
  mid-note, or a lock applied after the opcode, therefore never needs the
  allocator. Where a buffer would still have to grow, the opcode refuses in the
  same way.
- **Transforms.** The FFT convolutions and correlations refuse a new transform
  size on a marked path before the FFT setup is allocated. Their transform
  buffers follow the output's mark, which the output took from its operands at
  init, so `csnrtunlock` on the output releases them as well.
- **Csound arrays.** The k-rate forms of `csntoarray` and `csnshape` reserve
  their output at init, so a marked source that moves within its capacity never
  makes Csound grow the output array.
- **Strings.** Each element of a `CsnStr` keeps the buffer of its string, and a
  k form writes its strings into those buffers. On a marked string array a
  pass runs as long as every string fits the room its element already holds,
  and the first string that does not fit stops it with a performance error
  (`A string needs 22 bytes, its slot holds 4, ...`). The room is set at init:
  a marked output gets room for the longest string its operands hold, and the
  forms that make new strings (`csnstradd`, `csnstrmul`, a fill) size it from
  what they compute there. Writes in place (`csnset`, `csnsetslice`,
  `csnputmask`, a one-string `csnpush`) check every element first, so a pass
  that stops changes nothing. `csnload` and the in-place `csnpad` and block
  `csninsert` rebuild their strings on every pass and stay refused. See
  [String arrays](doc/string-arrays.md#real-time-paths).

Two things stay outside the guarantee. `csnsave`, `csnload` and `csnprint` do
file or console I/O on the performance thread, which no mark can make real-time
safe. Csound's own copy-on-write of an output array that has been shared with
another variable is Csound's to perform.

### Building a chain that never trips it

- **Settle shapes at init.** Every derived opcode sizes its output from its
  source at init, so a chain fed by an array of fixed shape allocates once per
  note and never again. A shape that depends on a k scalar (a pad width, a
  slice, a k axis) is not known at init (see
  [What the init pass computes](#what-the-init-pass-computes)): give it as a
  constant or an i variable when it does not move, or start its k variable at
  the largest value with `init`, which `csnpad` takes as the room to reserve.
- **Reserve what has to grow.** An array can grow up to its capacity without
  new storage. A derived output has room for twice the element count it had at
  init; one that has to vary further should start at init from the largest
  shape it will take. `csnempty` reserves a shape without publishing any
  element, which gives `csnpush` and an in-place resize room to grow into.
- **Give strings their longest value at init.** A marked string array writes
  into the room its elements took at init, so build it there with strings at
  least as long as the ones its k forms will write.
- **Unmark what does not go back out.** Frames harvested only for analysis do
  not need the guarantee: pass `irt = 0` at the audio source, or `csnrtunlock`
  the branch.

---

## Saving and loading

`csnsave` and `csnload` select the file format from the path extension. Use
`.csn` for csnum's own format or `.npy` to exchange arrays with NumPy. Other
extensions are rejected before a file is opened; the same i-rate and k-rate
opcode forms work with both formats.

The `.csn` format has a fixed 64-byte `CSDN` header followed by the raw array
payload. It records the version, element type, dimension count, shape, element
count and byte count. The loader checks these fields and restores the original
shape and real/complex type.

For `.npy`, `csnsave` writes a NumPy 3.0 file containing C-order `float64` or
`complex128` values. NumPy's `np.load` can read it. `csnload` accepts `.npy`
versions 1.0, 2.0 and 3.0 with numeric `bool`, signed/unsigned integers of
1/2/4/8 bytes, `float16`/`float32`/`float64`, or `complex64`/`complex128`.
It handles little, big and native byte order and converts Fortran-order payloads
to csnum's C-order layout. Real values become `double`; complex values become
pairs of `double`. Integer values beyond the exact range of `double` can lose
precision. Structured/object dtypes, scalar arrays with `shape=()`, and headers
over 10,000 bytes are not supported; csnum arrays have at most eight dimensions.
As with `.csn`, the element count must match the shape; an empty array with a
nonzero reserved shape cannot be saved as `.npy`.

Malformed headers, unsupported dtypes and incomplete payloads are rejected.
The [csnsave](doc/csnsave.md) and [csnload](doc/csnload.md) references give the
full argument and trigger behavior.

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

csnsave(data, "analysis.npy") // np.load("analysis.npy") can read this file
from_numpy:CsnArr = csnload("analysis.npy")
```

NumPy can also write the input file:

```python
import numpy as np
values = np.asfortranarray(np.array([[1, 2, 3], [4, 5, 6]], dtype=np.float32))
np.save("incoming.npy", values)
```

`csnload("incoming.npy")` returns the same 2×3 shape with C-order `double`
values.

At k-rate, reloading a file another process keeps rewriting. Until the first
trigger fires the output still holds the empty array the init pass published,
so a consumer that cannot read an empty extent belongs behind the trigger too:

```csound
instr 1
    trig:k       = metro(10)
    table:CsnArr = csnload("live.npy", trig)
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

The hop must be at least `ksmps`: one `CsnArr` variable holds one frame, so at most one
frame can be published per control period, and a smaller hop would overwrite a
frame before any consumer could read it. It is refused at init rather than
silently dropping frames. The default hop is the frame size, which gives
contiguous frames with no overlap.

`csnstream` folds in one frame per hop of output, counted on a phase accumulator
of its own, so any number of k-rate opcodes may sit between the two ends without
changing the result. With a rectangular window and `ihop` equal to the frame
length the reconstruction is exact; at 50% overlap every sample is covered twice,
so a real chain applies a window whose overlapped copies sum to one.

### Audio arrays are real-time paths

An array published by `csnfromaudio`, `csnpack` or `csnsnap` is marked as a
real-time path, and every array derived from it inherits the mark when it is
created. A chain whose shapes are settled at init, which is the ordinary case since
`csnfromaudio` fixes its shape at `ksmps`, allocates once per note and never
again. Where the frames are harvested for analysis rather than sent back out,
`irt = 0` at the source lifts the restriction for everything derived from it.
[Real-time paths](#real-time-paths) covers the mark in full.

---

## Examples

Sort and reduce, entirely at i-time:

```csound
data:CsnArr     = csnfromarray(array(4, 1, 3, 2))
sorted:CsnArr   = csnsort(data)
sorted_back:i[] = csntoarray(sorted) // 1 2 3 4
peak:i          = csnmax(data) // 4
```

Count integer bins, with an optional weight per input element:

```csound
bins:CsnArr     = csnfromarray(array(0, 1, 1, 3))
counts:CsnArr   = csnbincount(bins) // 1 2 0 1
weights:CsnArr  = csnfromarray(array(0.5, 1, 2, 4))
weighted:CsnArr = csnbincount(bins, weights) // 0.5 3 0 4
```

Find where values can be inserted without disturbing ascending order:

```csound
sorted:CsnArr = csnfromarray(array(1, 3, 3, 5))
wanted:CsnArr = csnfromarray(array(0, 3, 4, 6))
left:CsnArr   = csnsearchsorted(sorted, wanted)    // 0 1 3 4
right:CsnArr  = csnsearchsorted(sorted, wanted, 1) // 0 3 3 4
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
opcodes keep their previous result on all the others.

```csound
instr 1
    trig:k        = metro(200)
    scaled:CsnArr = csnmul(buffer, 0.5, trig)
    peak:k        = csnmax(scaled, trig)
    printf("peak=%.1f\n", trig, peak)
endin
```

In place, when a new array would be waste:

```csound
csnnormalize(data) // rewrites data; downstream consumers see a new generation
```

## Development note

All source code in this project is written and maintained by humans.
AI tools may be used only as development assistants for code review, debugging, documentation and testing. AI suggestions are reviewed and evaluated by the project maintainers before being incorporated.
