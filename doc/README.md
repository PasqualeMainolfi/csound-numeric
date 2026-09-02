# csnum opcode reference

One page per opcode: the full set of overloads, what each argument means, and a
runnable example. The same examples live under [`example/`](../example) as
standalone `.csd` files — every one of them runs, and the numbers quoted in the
documentation are the numbers it prints.

Run one with the plugin in place:

```sh
csound --opcode-dir=build example/csnsort.csd
```

Conventions that apply throughout: an optional axis argument defaults to `-1`,
meaning the array is read flat; opcodes that publish a new handle usually have an
in-place sibling under the same name that writes back into its source and returns
nothing; most k-rate forms take an optional trailing trigger, and a zero trigger
republishes the previous result instead of recomputing. A trigger that is the
only k-rate argument is required to select the performance overload, and
side-effecting opcodes such as `csnprint` simply do nothing when it is zero. See the
[README](../README.md) for the handle model and the k-rate versioning, and
[OPS_INDEX](../OPS_INDEX.md) for the same list with the rate and element-type
support of each opcode.

## Creation

- [csnempty](csnempty.md) - reserves a shape without publishing any element
- [csnzeros](csnzeros.md) - array of zeros
- [csnones](csnones.md) - array of ones
- [csnfull](csnfull.md) - array filled with one value
- [csnlike](csnlike.md) - array shaped and typed like another, filled with a value
- [csnidentity](csnidentity.md) - identity matrix
- [csnarange](csnarange.md) - evenly spaced values from start to stop by step
- [csnlinspace](csnlinspace.md) - n values evenly spaced between two bounds
- [csnlogspace](csnlogspace.md) - n values evenly spaced on a logarithmic scale
- [csngeomspace](csngeomspace.md) - n values in geometric progression
- [csnrand](csnrand.md) - uniform random values in a range
- [csnseed](csnseed.md) - seeds the random generator

## Conversion, lifetime and queries

- [csnfromarray](csnfromarray.md) - Csound array to handle
- [csntoarray](csntoarray.md) - handle to Csound array
- [csnfromftable](csnfromftable.md) - function table to handle
- [csntoftable](csntoftable.md) - handle to function table, optionally resizing it
- [csncopy](csncopy.md) - independent copy of an array
- [csnfree](csnfree.md) - releases the array behind a handle
- [csntype](csntype.md) - item type: 0 real, 1 complex
- [csndims](csndims.md) - number of dimensions
- [csnsize](csnsize.md) - number of elements
- [csnshape](csnshape.md) - extents, one per dimension
- [csnisempty](csnisempty.md) - 1 when the array holds no element
- [csnprint](csnprint.md) - prints shape, dtype and values in a NumPy-style layout

## Persistence

- [csnsave](csnsave.md) - writes an array to a `.csn` file
- [csnload](csnload.md) - reads an array back from a `.csn` file

## Shape and layout

- [csnreshape](csnreshape.md) - same elements under a new shape
- [csnflatten](csnflatten.md) - collapses every dimension into one
- [csntranspose](csntranspose.md) - permutes the axes
- [csnflip](csnflip.md) - reverses the order along an axis
- [csnroll](csnroll.md) - circular shift, whole array or along an axis
- [csnreverse](csnreverse.md) - reverses the flat element order
- [csnpad](csnpad.md) - adds elements before and after, filled with a value
- [csntruncate](csntruncate.md) - shortens one axis, or every axis, to a length
- [csnhead](csnhead.md) - keeps the first n elements of a 1-D array
- [csnresize](csnresize.md) - reshapes to any size, zero-filling what it grows
- [csnconcat](csnconcat.md) - joins two arrays, flat or along an axis
- [csninsert](csninsert.md) - inserts elements or a block at a position
- [csnremove](csnremove.md) - removes elements or a block at a position
- [csnpush](csnpush.md) - appends one element at the end
- [csnpop](csnpop.md) - removes the last element and returns it

## Indexing and selection

- [csnget](csnget.md) - reads one element by coordinates
- [csnset](csnset.md) - writes one element by coordinates
- [csngetslice](csngetslice.md) - extracts a strided slice along an axis
- [csnsetslice](csnsetslice.md) - writes into a strided slice along an axis
- [csntake](csntake.md) - picks one index along an axis, dropping that axis
- [csnargwhere](csnargwhere.md) - coordinates of the elements matching a value array
- [csnargnonzero](csnargnonzero.md) - coordinates of the non-zero elements
- [csnargisnan](csnargisnan.md) - coordinates of the NaN elements

## Elementwise arithmetic

- [csnadd](csnadd.md) - addition, array with array or with a scalar
- [csnsubtract](csnsubtract.md) - subtraction, array with array or with a scalar
- [csnmul](csnmul.md) - multiplication, array with array or with a scalar
- [csndiv](csndiv.md) - division, array with array or with a scalar
- [csnpow](csnpow.md) - power, array with array or with a scalar
- [csnlog](csnlog.md) - logarithm in an arbitrary base
- [csndivmod](csndivmod.md) - quotient and remainder in one pass, as two arrays
- [csnhypot](csnhypot.md) - hypotenuse of two arrays, elementwise
- [csnclip](csnclip.md) - clamps every element between two bounds
- [csnabs](csnabs.md) - absolute value, or magnitude for a complex array
- [csnsign](csnsign.md) - -1, 0 or 1 by the sign of each element
- [csnfloor](csnfloor.md) - rounds each element down
- [csnceil](csnceil.md) - rounds each element up
- [csnround](csnround.md) - rounds each element to the nearest integer

## Transcendental functions

- [csnexp](csnexp.md) - exponential
- [csnsqrt](csnsqrt.md) - square root
- [csncbrt](csncbrt.md) - cube root
- [csnsin](csnsin.md) - sine
- [csncos](csncos.md) - cosine
- [csntan](csntan.md) - tangent
- [csnasin](csnasin.md) - arcsine
- [csnacos](csnacos.md) - arccosine
- [csnatan](csnatan.md) - arctangent
- [csnsinh](csnsinh.md) - hyperbolic sine
- [csncosh](csncosh.md) - hyperbolic cosine
- [csntanh](csntanh.md) - hyperbolic tangent
- [csnasinh](csnasinh.md) - inverse hyperbolic sine
- [csnacosh](csnacosh.md) - inverse hyperbolic cosine
- [csnatanh](csnatanh.md) - inverse hyperbolic tangent

## Angles and phase

- [csndegtorad](csndegtorad.md) - degrees to radians
- [csnradtodeg](csnradtodeg.md) - radians to degrees
- [csnwrap](csnwrap.md) - wraps values into one period
- [csnunwrap](csnunwrap.md) - removes the jumps left by wrapping, along an axis

## Comparison and logic

- [csngt](csngt.md) - elementwise greater than, as a 0/1 array
- [csnlt](csnlt.md) - elementwise less than, as a 0/1 array
- [csnge](csnge.md) - elementwise greater or equal, as a 0/1 array
- [csnle](csnle.md) - elementwise less or equal, as a 0/1 array
- [csneq](csneq.md) - elementwise equality, as a 0/1 array
- [csnne](csnne.md) - elementwise inequality, as a 0/1 array
- [csnlogicand](csnlogicand.md) - logical and of two arrays, or of an array and a scalar
- [csnlogicor](csnlogicor.md) - logical or of two arrays, or of an array and a scalar
- [csnlogicnot](csnlogicnot.md) - logical negation
- [csnall](csnall.md) - 1 when every element is non-zero
- [csnany](csnany.md) - 1 when at least one element is non-zero
- [csncnteq](csncnteq.md) - counts the elements equal to a value
- [csncntnz](csncntnz.md) - counts the non-zero elements
- [csncntnan](csncntnan.md) - counts the NaN elements

## Reductions and statistics

- [csnsum](csnsum.md) - sum, over everything or along an axis
- [csnprod](csnprod.md) - product, over everything or along an axis
- [csnsub](csnsub.md) - running subtraction of every element from the first
- [csnmean](csnmean.md) - arithmetic mean
- [csnmin](csnmin.md) - smallest element
- [csnmax](csnmax.md) - largest element
- [csnmedian](csnmedian.md) - median
- [csnstd](csnstd.md) - standard deviation
- [csnvar](csnvar.md) - variance
- [csnpercentile](csnpercentile.md) - percentile, from 0 to 100
- [csnquantile](csnquantile.md) - quantile, from 0 to 1
- [csnargmin](csnargmin.md) - coordinates of the smallest element
- [csnargmax](csnargmax.md) - coordinates of the largest element
- [csncumsum](csncumsum.md) - cumulative sum
- [csncumprod](csncumprod.md) - cumulative product
- [csndiff](csndiff.md) - differences between consecutive elements
- [csngrad](csngrad.md) - central-difference gradient
- [csnmovmean](csnmovmean.md) - moving average over a window
- [csnmovmedian](csnmovmedian.md) - moving median over a window
- [csnmovmin](csnmovmin.md) - moving minimum over a window
- [csnmovmax](csnmovmax.md) - moving maximum over a window
- [csnmovstd](csnmovstd.md) - moving standard deviation over a window
- [csnmovvar](csnmovvar.md) - moving variance over a window

## Sorting and sets

- [csnsort](csnsort.md) - sorts, over everything or along an axis
- [csnargsort](csnargsort.md) - the coordinates that would sort the array
- [csnunique](csnunique.md) - the distinct values, in order
- [csnargunique](csnargunique.md) - coordinates of the first occurrence of each value

## Linear algebra and geometry

- [csndot](csndot.md) - NumPy-style dot: scalar product of vectors, matrix product of matrices
- [csninner](csninner.md) - inner product: sum over the last axis of both operands
- [csnouter](csnouter.md) - outer product of two vectors
- [csnmatmul](csnmatmul.md) - matrix multiplication
- [csntrace](csntrace.md) - sum of the diagonal
- [csndiag](csndiag.md) - diagonal of a matrix, or a matrix from a diagonal
- [csnnorm](csnnorm.md) - vector or matrix norm of a given order
- [csnnormalize](csnnormalize.md) - divides by its own norm of a given order (sum of magnitudes by default)
- [csncross](csncross.md) - cross product of two 3-element vectors
- [csndist](csndist.md) - Minkowski distance between two arrays, of a given order
- [csnpairdist](csnpairdist.md) - elementwise distance between two arrays of the same shape
- [csnangledist](csnangledist.md) - angle between two vectors
- [csnproject](csnproject.md) - component of one vector along another
- [csnreject](csnreject.md) - component of one vector orthogonal to another
- [csnreflect](csnreflect.md) - reflects a vector about another

## Complex arrays

- [csnreal](csnreal.md) - real parts, as a real array
- [csnimag](csnimag.md) - imaginary parts, as a real array
- [csnangle](csnangle.md) - argument of each element
- [csnconj](csnconj.md) - complex conjugate
- [csntocomplex](csntocomplex.md) - real array to complex, imaginary parts at zero
- [csntoreal](csntoreal.md) - complex array to real, keeping the real parts

## Interpolation and resampling

- [csninterp](csninterp.md) - maps values through a breakpoint table, five interpolation modes
- [csnresample](csnresample.md) - resamples an array to a new length along one axis

## Windows

- [csnhanning](csnhanning.md) - Hann window
- [csnhamming](csnhamming.md) - Hamming window
- [csnbartlett](csnbartlett.md) - Bartlett (triangular) window
- [csnblackman](csnblackman.md) - Blackman window
- [csnkaiser](csnkaiser.md) - Kaiser window with a beta parameter

## Audio bridge

These run at performance time on the audio path. Their arrays are marked as
realtime by default, which means neither they nor anything derived from them may
reallocate during performance; pass `irt = 0` at the source to lift that where a
chain is being harvested for analysis rather than sent back out.

- [csnfromaudio](csnfromaudio.md) - one control period of an audio signal into an array
- [csntoaudio](csntoaudio.md) - an array of ksmps elements back out as audio
- [csnpack](csnpack.md) - an array of audio signals into one channels x ksmps array
- [csnunpack](csnunpack.md) - that array back into one signal per channel
- [csnsnap](csnsnap.md) - slices a stream into overlapping frames of a chosen size
- [csnstream](csnstream.md) - overlap-adds frames back into a continuous signal
- [csnrtlock](csnrtlock.md) - marks any handle as a real-time path, or clears the mark
