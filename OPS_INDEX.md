# csnum opcode index

One line per opcode, grouped by family. Overloads are not listed: an opcode
appears once under its plain name, whatever the number of type and rate
variants behind it. The parentheses carry two things: the rate, `i, k` for the
opcodes with both an i-time and a performance-time form, `i` for the ones that
only run at init, and the element types accepted, `real, complex` or
`real only`. The five opcodes that read a complex array and hand back its parts
are marked `complex only`, since a real input has nothing for them to do.

Conventions that apply throughout: an optional axis argument defaults to `-1`,
meaning the array is read flat; opcodes that publish a new handle usually have
an in-place sibling under the same name that writes back into its source and
returns nothing; most k-rate forms take an optional trailing trigger, and a zero
trigger republishes the previous result instead of recomputing. A trigger that
is the only k-rate argument is required to select the performance overload;
side-effecting opcodes simply do nothing when it is zero. See the README for the
handle model and the k-rate versioning.

## Creation

- **csnempty** - reserves a shape without publishing any element (i, k — real, complex)
- **csnzeros** - array of zeros (i, k — real, complex)
- **csnones** - array of ones (i, k — real, complex)
- **csnfull** - array filled with one value (i, k — real, complex)
- **csnlike** - array shaped and typed like another, filled with a value (i, k — real, complex)
- **csnidentity** - identity matrix (i, k — real, complex)
- **csnarange** - evenly spaced values from start to stop by step (i, k — real only)
- **csnlinspace** - n values evenly spaced between two bounds (i, k — real only)
- **csnlogspace** - n values evenly spaced on a logarithmic scale (i, k — real only)
- **csngeomspace** - n values in geometric progression (i, k — real only)
- **csnrand** - uniform random values in a range (i, k — real only)
- **csnseed** - seeds the random generator (i)

## Conversion, lifetime and queries

- **csnfromarray** - Csound array to handle (i, k — real, complex)
- **csntoarray** - handle to Csound array (i, k — real, complex)
- **csnfromftable** - function table to handle (i — real only)
- **csntoftable** - handle to function table, optionally resizing it (i — real only)
- **csncopy** - independent copy of an array (i, k — real, complex)
- **csnfree** - releases the array behind a handle (i — real, complex)
- **csntype** - item type: 0 real, 1 complex (i, k — real, complex)
- **csndims** - number of dimensions (i, k — real, complex)
- **csnsize** - number of elements (i, k — real, complex)
- **csnshape** - extents, one per dimension (i, k — real, complex)
- **csnisempty** - 1 when the array holds no element (i, k — real, complex)
- **csnprint** - prints shape, dtype and values in a NumPy-style layout (i, k — real, complex)

## Persistence

Both take a path that must end in `.csn`. The format stores the element type,
the shape and the payload, so an array survives the round trip unchanged; a
file written by a newer major or minor version is refused rather than guessed
at. Unlike the rest of the suite, a zero trigger here does not republish a
previous result — it simply does not touch the disk.

- **csnsave** - writes an array to a `.csn` file (i, k — real, complex)
- **csnload** - reads an array back from a `.csn` file (i, k — real, complex)

## Shape and layout

- **csnreshape** - same elements under a new shape (i, k — real, complex)
- **csnflatten** - collapses every dimension into one (i, k — real, complex)
- **csntranspose** - permutes the axes (i, k — real, complex)
- **csnflip** - reverses the order along an axis (i, k — real, complex)
- **csnroll** - circular shift, whole array or along an axis (i, k — real, complex)
- **csnreverse** - reverses the flat element order (i, k — real, complex)
- **csnpad** - adds elements before and after, filled with a value (i, k — real, complex)
- **csntruncate** - shortens one axis, or every axis, to a length (i, k — real, complex)
- **csnhead** - keeps the first n elements of a 1-D array (i, k — real, complex)
- **csnresize** - reshapes to any size, zero-filling what it grows (i, k — real, complex)
- **csnconcat** - joins two arrays, flat or along an axis (i, k — real, complex)
- **csninsert** - inserts elements or a block at a position (i, k — real, complex)
- **csnremove** - removes elements or a block at a position (i, k — real, complex)
- **csnpush** - appends one element at the end (i, k — real, complex)
- **csnpop** - removes the last element and returns it (i, k — real, complex)

## Indexing and selection

- **csnget** - reads one element by coordinates (i, k — real, complex)
- **csnset** - writes one element by coordinates (i, k — real, complex)
- **csngetslice** - extracts a strided slice along an axis (i, k — real, complex)
- **csnsetslice** - writes into a strided slice along an axis (i, k — real, complex)
- **csntake** - picks one index along an axis, dropping that axis (i, k — real, complex)
- **csnargwhere** - coordinates of the elements matching a value array (i, k — real only)
- **csnargnonzero** - coordinates of the non-zero elements (i, k — real only)
- **csnargisnan** - coordinates of the NaN elements (i, k — real only)

## Elementwise arithmetic

- **csnadd** - addition, array with array or with a scalar (i, k — real, complex)
- **csnsubtract** - subtraction, array with array or with a scalar (i, k — real, complex)
- **csnmul** - multiplication, array with array or with a scalar (i, k — real, complex)
- **csndiv** - division, array with array or with a scalar (i, k — real, complex)
- **csnpow** - power, array with array or with a scalar (i, k — real, complex)
- **csnlog** - logarithm in an arbitrary base (i, k — real, complex)
- **csndivmod** - quotient and remainder in one pass, as two arrays (i, k — real only)
- **csnhypot** - hypotenuse of two arrays, elementwise (i, k — real only)
- **csnclip** - clamps every element between two bounds (i, k — real only)
- **csnabs** - absolute value, or magnitude for a complex array (i, k — real, complex)
- **csnsign** - -1, 0 or 1 by the sign of each element (i, k — real, complex)
- **csnfloor** - rounds each element down (i, k — real only)
- **csnceil** - rounds each element up (i, k — real only)
- **csnround** - rounds each element to the nearest integer (i, k — real only)

## Transcendental functions

- **csnexp** - exponential (i, k — real, complex)
- **csnsqrt** - square root (i, k — real, complex)
- **csncbrt** - cube root (i, k — real, complex)
- **csnsin** - sine (i, k — real, complex)
- **csncos** - cosine (i, k — real, complex)
- **csntan** - tangent (i, k — real, complex)
- **csnasin** - arcsine (i, k — real, complex)
- **csnacos** - arccosine (i, k — real, complex)
- **csnatan** - arctangent (i, k — real, complex)
- **csnsinh** - hyperbolic sine (i, k — real, complex)
- **csncosh** - hyperbolic cosine (i, k — real, complex)
- **csntanh** - hyperbolic tangent (i, k — real, complex)
- **csnasinh** - inverse hyperbolic sine (i, k — real, complex)
- **csnacosh** - inverse hyperbolic cosine (i, k — real, complex)
- **csnatanh** - inverse hyperbolic tangent (i, k — real, complex)

## Angles and phase

- **csndegtorad** - degrees to radians (i, k — real only)
- **csnradtodeg** - radians to degrees (i, k — real only)
- **csnwrap** - wraps values into one period (i, k — real, complex)
- **csnunwrap** - removes the jumps left by wrapping, along an axis (i, k — real, complex)

## Comparison and logic

- **csngt** - elementwise greater than, as a 0/1 array (i, k — real only)
- **csnlt** - elementwise less than, as a 0/1 array (i, k — real only)
- **csnge** - elementwise greater or equal, as a 0/1 array (i, k — real only)
- **csnle** - elementwise less or equal, as a 0/1 array (i, k — real only)
- **csneq** - elementwise equality, as a 0/1 array (i, k — real only)
- **csnne** - elementwise inequality, as a 0/1 array (i, k — real only)
- **csnlogicand** - logical and of two arrays, or of an array and a scalar (i, k — real only)
- **csnlogicor** - logical or of two arrays, or of an array and a scalar (i, k — real only)
- **csnlogicnot** - logical negation (i, k — real only)
- **csnall** - 1 when every element is non-zero (i, k — real, complex)
- **csnany** - 1 when at least one element is non-zero (i, k — real, complex)
- **csncnteq** - counts the elements equal to a value (i, k — real only)
- **csncntnz** - counts the non-zero elements (i, k — real only)
- **csncntnan** - counts the NaN elements (i, k — real only)

## Reductions and statistics

- **csnsum** - sum, over everything or along an axis (i, k — real, complex)
- **csnprod** - product, over everything or along an axis (i, k — real, complex)
- **csnsub** - running subtraction of every element from the first (i, k — real, complex)
- **csnmean** - arithmetic mean (i, k — real, complex)
- **csnmin** - smallest element (i, k — real only)
- **csnmax** - largest element (i, k — real only)
- **csnmedian** - median (i, k — real only)
- **csnstd** - standard deviation (i, k — real, complex)
- **csnvar** - variance (i, k — real, complex)
- **csnpercentile** - percentile, from 0 to 100 (i, k — real only)
- **csnquantile** - quantile, from 0 to 1 (i, k — real only)
- **csnargmin** - coordinates of the smallest element (i, k — real only)
- **csnargmax** - coordinates of the largest element (i, k — real only)
- **csncumsum** - cumulative sum (i, k — real, complex)
- **csncumprod** - cumulative product (i, k — real, complex)
- **csndiff** - differences between consecutive elements (i, k — real, complex)
- **csngrad** - central-difference gradient (i, k — real only)
- **csnmovmean** - moving average over a window (i, k — real, complex)
- **csnmovmedian** - moving median over a window (i, k — real only)
- **csnmovmin** - moving minimum over a window (i, k — real only)
- **csnmovmax** - moving maximum over a window (i, k — real only)
- **csnmovstd** - moving standard deviation over a window (i, k — real, complex)
- **csnmovvar** - moving variance over a window (i, k — real, complex)

## Sorting and sets

- **csnsort** - sorts, over everything or along an axis (i, k — real only)
- **csnargsort** - the coordinates that would sort the array (i, k — real only)
- **csnunique** - the distinct values, in order (i, k — real only)
- **csnargunique** - coordinates of the first occurrence of each value (i, k — real only)

## Linear algebra and geometry

- **csndot** - NumPy-style dot: scalar product of vectors, matrix product of matrices (i, k — real, complex)
- **csninner** - inner product: sum over the last axis of both operands (i, k — real, complex)
- **csnouter** - outer product of two vectors (i, k — real, complex)
- **csnmatmul** - matrix multiplication (i, k — real, complex)
- **csntrace** - sum of the diagonal (i, k — real, complex)
- **csndiag** - diagonal of a matrix, or a matrix from a diagonal (i, k — real, complex)
- **csnnorm** - vector or matrix norm of a given order (i, k — real, complex)
- **csnnormalize** - divides by its own norm of a given order (sum of magnitudes by default) (i, k — real, complex)
- **csncross** - cross product of two 3-element vectors (i, k — real only)
- **csndist** - Minkowski distance between two arrays, of a given order (i, k — real, complex)
- **csnpairdist** - elementwise distance between two arrays of the same shape (i, k — real, complex)
- **csnangledist** - angle between two vectors (i, k — real, complex)
- **csnproject** - component of one vector along another (i, k — real only)
- **csnreject** - component of one vector orthogonal to another (i, k — real only)
- **csnreflect** - reflects a vector about another (i, k — real, complex)

## Complex arrays

- **csnreal** - real parts, as a real array (i, k — complex only)
- **csnimag** - imaginary parts, as a real array (i, k — complex only)
- **csnangle** - argument of each element (i, k — complex only)
- **csnconj** - complex conjugate (i, k — complex only)
- **csntocomplex** - real array to complex, imaginary parts at zero (i, k — real only)
- **csntoreal** - complex array to real, keeping the real parts (i, k — complex only)

## Interpolation and resampling

- **csninterp** - maps values through a breakpoint table, five interpolation modes (i, k — real only)
- **csnresample** - resamples an array to a new length along one axis (i, k — real only)

## Windows

- **csnhanning** - Hann window (i, k — real only)
- **csnhamming** - Hamming window (i, k — real only)
- **csnbartlett** - Bartlett (triangular) window (i, k — real only)
- **csnblackman** - Blackman window (i, k — real only)
- **csnkaiser** - Kaiser window with a beta parameter (i, k — real only)
