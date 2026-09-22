# csnum opcode reference

One page per opcode: the full set of overloads, what each argument means, and a
runnable example. The same examples live under [`example/`](../example) as
standalone `.csd` files — every one of them runs, and the numbers quoted in the
documentation are the numbers it prints.

Run one with the plugin in place:

```sh
csound --opcode-dir=build example/csnsort.csd
```

Conventions that apply throughout: explicit axes use NumPy indexing (`-1` is
the last axis, `-2` the penultimate), while omitting the axis selects the
operation's documented flat, all-axes, or last-axis default; opcodes that publish a new handle usually have an
in-place sibling under the same name that writes back into its source and returns
nothing; when an axis is optional, k-rate forms put the trigger before an
explicit axis, and a zero trigger
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
- [csnrandint](csnrandint.md) - uniform random integers in a half-open range
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

- [csnsave](csnsave.md) - writes an array to a `.csn` or NumPy `.npy` file
- [csnload](csnload.md) - reads a `.csn` or supported numeric `.npy` file

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
- [csnstack](csnstack.md) - inserts a new axis and stacks two or more equal-shaped arrays
- [csninsert](csninsert.md) - inserts elements or a block at a position
- [csnremove](csnremove.md) - removes elements or a block at a position
- [csnpush](csnpush.md) - appends one element at the end
- [csnpop](csnpop.md) - removes the last element and returns it

## Indexing and selection

- [csnget](csnget.md) - reads one element by coordinates
- [csngetrow](csngetrow.md) - extracts one row of a 2-D array as a vector
- [csngetcol](csngetcol.md) - extracts one column of a 2-D array as a vector
- [csnset](csnset.md) - writes one element by coordinates
- [csngetslice](csngetslice.md) - extracts a strided slice along an axis
- [csnsetslice](csnsetslice.md) - writes into a strided slice along an axis
- [csntake](csntake.md) - picks one index along an axis, dropping that axis
- [csnwhere](csnwhere.md) - picks from two arrays element by element, driven by a mask
- [csnputmask](csnputmask.md) - the same choice made in place, the array being its own mask
- [csncompress](csncompress.md) - keeps the entries a 1-D mask marks, along an axis or flat
- [csnselect](csnselect.md) - keeps the elements a mask marks, as a flat array
- [csnindexof](csnindexof.md) - coordinates of the first element equal to a scalar, or an empty array
- [csnbincount](csnbincount.md) - counts every non-negative integer bin, optionally accumulating weights
- [csnsearchsorted](csnsearchsorted.md) - insertion indices in an ascending 1-D array, left or right of equal values
- [csnargwhere](csnargwhere.md) - coordinates of the elements matching a value array
- [csnargnonzero](csnargnonzero.md) - coordinates of the non-zero elements
- [csnargisnan](csnargisnan.md) - coordinates of the NaN elements
- [csnnext](csnnext.md) - hands back one element per control period, walking the array flat

## Elementwise arithmetic

- [csnadd](csnadd.md) - addition, array with array or with a scalar
- [csnsubtract](csnsubtract.md) - subtraction, array with array or with a scalar
- [csnmul](csnmul.md) - multiplication, array with array or with a scalar
- [csndiv](csndiv.md) - division, array with array or with a scalar
- [csnpow](csnpow.md) - power, array with array or with a scalar
- [csnlog](csnlog.md) - logarithm in an arbitrary base
- [csndivmod](csndivmod.md) - quotient and remainder in one pass, as two arrays
- [csnhypot](csnhypot.md) - hypotenuse of two arrays, elementwise
- [csnminimum](csnminimum.md) - elementwise minimum of two arrays, or of an array and a scalar
- [csnmaximum](csnmaximum.md) - elementwise maximum of two arrays, or of an array and a scalar
- [csnclip](csnclip.md) - clamps every element between two bounds
- [csnabs](csnabs.md) - absolute value, or magnitude for a complex array
- [csnsign](csnsign.md) - -1, 0 or 1 by the sign of each element
- [csnfloor](csnfloor.md) - rounds each element down
- [csnceil](csnceil.md) - rounds each element up
- [csnround](csnround.md) - rounds each element to the nearest integer
- [csnforeach](csnforeach.md) - maps a user-defined opcode over every element, in place

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
- [csnatan2](csnatan2.md) - two-argument arctangent, quadrant aware
- [csnwrap](csnwrap.md) - wraps values into one period
- [csnunwrap](csnunwrap.md) - removes the jumps left by wrapping, along an axis

## Comparison and logic

- [csngt](csngt.md) - elementwise greater than, as a 0/1 array
- [csnlt](csnlt.md) - elementwise less than, as a 0/1 array
- [csnge](csnge.md) - elementwise greater or equal, as a 0/1 array
- [csnle](csnle.md) - elementwise less or equal, as a 0/1 array
- [csneq](csneq.md) - elementwise equality, as a 0/1 array
- [csnne](csnne.md) - elementwise inequality, as a 0/1 array
- [csnisnan](csnisnan.md) - 1 where an element is NaN, otherwise 0
- [csnisinf](csnisinf.md) - 1 where an element is positive or negative infinity, otherwise 0
- [csnisfin](csnisfin.md) - 1 where an element is finite, otherwise 0
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
- [csnrms](csnrms.md) - root mean square, over everything or along an axis
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
- [csnmedfilt1d](csnmedfilt1d.md) - median filter along one axis, zero-padded edges
- [csnmedfilt](csnmedfilt.md) - N-D median filter over a box, zero-padded edges
- [csnmovmin](csnmovmin.md) - moving minimum over a window
- [csnmovmax](csnmovmax.md) - moving maximum over a window
- [csnmovstd](csnmovstd.md) - moving standard deviation over a window
- [csnmovvar](csnmovvar.md) - moving variance over a window

## Sorting and sets

Set operations require a one-dimensional real array in ascending,
duplicate-free form. Only the set API guarantees this invariant; generic array
operations do not. Use `csnlikeset` again after a generic modification. See
[Set arrays and their invariant](set-arrays.md).

- [csnshuffle](csnshuffle.md) - randomly permutes the flat element order in place
- [csnsort](csnsort.md) - sorts, over everything or along an axis
- [csnargsort](csnargsort.md) - the coordinates that would sort the array
- [csnunique](csnunique.md) - the distinct values, in order
- [csnargunique](csnargunique.md) - coordinates of the first occurrence of each value
- [csnlikeset](csnlikeset.md) - normalizes an array as a sorted, duplicate-free set
- [csnunlikeset](csnunlikeset.md) - removes the set classification without changing values
- [csnsetinsert](csnsetinsert.md) - inserts a member while preserving the set invariant
- [csnsetremove](csnsetremove.md) - removes a member while preserving the set invariant
- [csnsetcontains](csnsetcontains.md) - tests membership
- [csnsetunion](csnsetunion.md) - values belonging to either set
- [csnsetintersect](csnsetintersect.md) - values common to both sets
- [csnsetdiff](csnsetdiff.md) - values in the first set but not the second
- [csnsetsymdiff](csnsetsymdiff.md) - values belonging to exactly one set
- [csnsetissubset](csnsetissubset.md) - tests whether the first set is a subset
- [csnsetissuperset](csnsetissuperset.md) - tests whether the first set is a superset
- [csnsetisdisjoint](csnsetisdisjoint.md) - tests whether two sets share no value
- [csnsetisequal](csnsetisequal.md) - tests whether two sets have the same members

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
- [csnsolve](csnsolve.md) - solves a linear system A x = B
- [csninv](csninv.md) - inverse of a square matrix
- [csndet](csndet.md) - determinant of a square matrix
- [csnsavgol](csnsavgol.md) - Savitzky-Golay coefficient matrix, one row per derivative order
- [csnhilbertmat](csnhilbertmat.md) - the Hilbert matrix, the textbook ill-conditioned example

## Complex arrays
`csnadd`, `csnsubtract`, `csnmul`, `csndiv`, `csnpow` and `csnsqrt` also take
scalar `:Complex;` operands, and `csnabs`, `csnangle` and `csnconj` a single
one, with no handle and no trigger involved. Those forms are listed on the
opcode's own page rather than as separate entries here.

- [csnreal](csnreal.md) - real parts, as a real array
- [csnimag](csnimag.md) - imaginary parts, as a real array
- [csnangle](csnangle.md) - argument of each element
- [csnconj](csnconj.md) - complex conjugate
- [csntocomplex](csntocomplex.md) - real array to complex, imaginary parts at zero
- [csntoreal](csntoreal.md) - complex array to real, keeping the real parts

## Fourier analysis

- [csnfft](csnfft.md) - full complex FFT along one axis
- [csnrfft](csnrfft.md) - one-sided FFT of a real array
- [csnifft](csnifft.md) - inverse full complex FFT
- [csnirfft](csnirfft.md) - real inverse of a one-sided spectrum
- [csnfft2](csnfft2.md) - two-dimensional full complex FFT
- [csnrfft2](csnrfft2.md) - two-dimensional real FFT with a one-sided last axis
- [csnifft2](csnifft2.md) - two-dimensional inverse complex FFT
- [csnirfft2](csnirfft2.md) - real inverse of a two-dimensional one-sided spectrum
- [csnstft](csnstft.md) - short-time Fourier transform with frequency and time coordinates
- [csnistft](csnistft.md) - inverse STFT with window-normalized overlap-add
- [csnfftfreq](csnfftfreq.md) - coordinates for a full FFT spectrum
- [csnrfftfreq](csnrfftfreq.md) - coordinates for a one-sided real spectrum
- [csnfftshift](csnfftshift.md) - moves zero frequency to the centre of an axis
- [csnifftshift](csnifftshift.md) - restores native FFT ordering
- [csndctone1d](csndctone1d.md) - DCT-I along one axis
- [csndcttwo1d](csndcttwo1d.md) - DCT-II along one axis
- [csndstone1d](csndstone1d.md) - DST-I along one axis
- [csndsttwo1d](csndsttwo1d.md) - DST-II along one axis
- [csnhilbert1d](csnhilbert1d.md) - analytic signal of a real array along one axis
- [csnhilbert1dr](csnhilbert1dr.md) - the Hilbert transform itself, real in and real out
- [csnhilbert2](csnhilbert2.md) - two-dimensional analytic signal

## Cepstral and mel analysis

- [csnmfcc](csnmfcc.md) - mel-frequency cepstral coefficients of a signal
- [csnmfbank](csnmfbank.md) - triangular mel filterbank matrix
- [csnmlogfbank](csnmlogfbank.md) - the same matrix in logarithmic scale

## Convolution and correlation

- [csnconvolve1d](csnconvolve1d.md) - convolution with a 1-D kernel, flat or along one axis
- [csncorrelate1d](csncorrelate1d.md) - cross-correlation with a 1-D kernel, flat or along one axis
- [csnconvolve](csnconvolve.md) - N-D convolution with a kernel of the same rank
- [csncorrelate](csncorrelate.md) - N-D cross-correlation with a kernel of the same rank
- [csnfftconvolve1d](csnfftconvolve1d.md) - the same convolution through Fourier transforms
- [csnfftcorrelate1d](csnfftcorrelate1d.md) - the same cross-correlation through Fourier transforms
- [csnfftconvolve](csnfftconvolve.md) - N-D convolution through Fourier transforms
- [csnfftcorrelate](csnfftcorrelate.md) - N-D cross-correlation through Fourier transforms

## Interpolation and resampling

- [csninterp](csninterp.md) - maps values through a breakpoint table, five interpolation modes
- [csnresample](csnresample.md) - resamples an array to a new length along one axis

## Windows

- [csnhanning](csnhanning.md) - Hann window
- [csnhamming](csnhamming.md) - Hamming window
- [csnbartlett](csnbartlett.md) - Bartlett (triangular) window
- [csnblackman](csnblackman.md) - Blackman window
- [csnkaiser](csnkaiser.md) - Kaiser window with a beta parameter

## Coordinate systems

Conversions between cartesian coordinates and the polar, cylindrical and
spherical systems, each pair named for its own so nothing depends on how many
arguments were passed. The spherical order is `(r, theta, phi)` with `theta` the
inclination from `+z`, and both directions of a pair use it, so a conversion and
its inverse return the point they started from. An optional trailing flag reads
`0` for radians, the default, and `1` for degrees, on whichever side of the
conversion carries the angles.

- [csnpoltocar](csnpoltocar.md) - polar to cartesian, in the plane
- [csncartopol](csncartopol.md) - cartesian to polar, in the plane
- [csncyltocar](csncyltocar.md) - cylindrical to cartesian, the height carried through
- [csncartocyl](csncartocyl.md) - cartesian to cylindrical
- [csnsphtocar](csnsphtocar.md) - spherical to cartesian
- [csncartosph](csncartosph.md) - cartesian to spherical
- [csnhoatocar](csnhoatocar.md) - ambisonics direction to cartesian, elevation from the horizon
- [csncartohoa](csncartohoa.md) - cartesian to ambisonics direction
- [csnrotmat](csnrotmat.md) - 3 x 3 rotation matrix, about a basis axis or an arbitrary direction
- [csnrotmatypr](csnrotmatypr.md) - one 3 x 3 matrix composing a yaw, a pitch and a roll
- [csnnmtoacn](csnnmtoacn.md) - ACN channel index from an order and a degree
- [csnacntonm](csnacntonm.md) - the order and degree an ACN index stands for
- [csnhoaordtochnls](csnhoaordtochnls.md) - channels in a complete 3-D set of a given order
- [csnchnlstohoaord](csnchnlstohoaord.md) - the order a channel count stands for, perfect squares only
- [csnsn3dton3d](csnsn3dton3d.md) - per-channel gains from SN3D to N3D, indexed by ACN
- [csnn3dtosn3d](csnn3dtosn3d.md) - per-channel gains from N3D to SN3D, indexed by ACN

## Room acoustics

Estimates a designer works with before there is any audio: reverberation time
from a room's surfaces, the absorption needed to reach a target time, the
frequency above which a room stops being modal, and the modal frequencies
themselves.

- [csnt60sab](csnt60sab.md) - Sabine reverberation time from a volume and its absorbing surfaces
- [csnt60eyr](csnt60eyr.md) - Eyring-Norris reverberation time, the model that still holds in an absorbent room
- [csnrt60absp](csnrt60absp.md) - the absorption a room needs to reach a target reverberation time
- [csnfschrd](csnfschrd.md) - Schroeder frequency, the crossover between the modal and diffuse regions
- [csnfpqr](csnfpqr.md) - resonant frequency of a rectangular room for one modal index triple
- [csnt60tofbg](csnt60tofbg.md) - the feedback gain a comb of a given delay needs to decay by 60 dB in a given time
- [csnfbgtot60](csnfbgtot60.md) - the decay time of a comb of a given delay at a given feedback gain

## Levels and time conversion

`csndbsum` adds levels by their powers, which is the only way decibels add. The
four conversions take the sample rate as an argument rather than reading the
orchestra's, so material carrying a rate of its own converts against that rate.

- [csndbsum](csndbsum.md) - sums levels in dB by adding their powers, over a pair, an array, or one axis
- [csnsamptomillis](csnsamptomillis.md) - sample counts into milliseconds at a given rate
- [csnsamptosec](csnsamptosec.md) - sample counts into seconds at a given rate
- [csnmillistosamp](csnmillistosamp.md) - milliseconds into sample counts at a given rate
- [csnsectosamp](csnsectosamp.md) - seconds into sample counts at a given rate

## Audio bridge

These run at performance time on the audio path. Their arrays are marked as
real-time by default, which means neither they nor anything derived from them may
reallocate during performance; pass `irt = 0` at the source to lift that where a
chain is being harvested for analysis rather than sent back out.

- [csnfromaudio](csnfromaudio.md) - one control period of an audio signal into an array
- [csntoaudio](csntoaudio.md) - an array of ksmps elements back out as audio
- [csnpack](csnpack.md) - an array of audio signals into one channels x ksmps array
- [csnunpack](csnunpack.md) - that array back into one signal per channel
- [csnsnap](csnsnap.md) - slices a stream into overlapping frames of a chosen size
- [csnstream](csnstream.md) - overlap-adds frames back into a continuous signal
- [csnrtlock](csnrtlock.md) - marks any handle as a real-time path
- [csnrtunlock](csnrtunlock.md) - clears the real-time mark from one handle
- [csnrtlockstart](csnrtlockstart.md) - starts marking every array this note creates
- [csnrtlockend](csnrtlockend.md) - stops that marking; the note ending stops it too
- [csnrtlockall](csnrtlockall.md) - marks every array for the whole performance, header only
