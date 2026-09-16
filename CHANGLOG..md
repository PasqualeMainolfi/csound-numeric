# Changelog

## [0.1.4] - 

- Refactor the former monolithic `csnum.c` into category-focused implementation units for creation, shape transforms, indexing and mutation, selection, statistics, element-wise math, vector operations, signal utilities, interpolation, file I/O, audio bridging, and filtering; keep the complete `OENTRY` inventory centralized in `csnum.c` and make cross-module dependencies explicit through an internal interface
- Add first-occurrence lookup by scalar value, returning the element's full coordinate vector or an empty array when it is absent (*csnindexof*)
- Add NumPy-style counting of non-negative integer bins, optionally accumulating one weight per source element, at init and k-rate (*csnbincount*)
- Add NumPy-style insertion-point search in ascending one-dimensional arrays, for scalar or array queries, with left/right duplicate placement and version-aware k-rate forms (*csnsearchsorted*)
- Add opcode reference pages, runnable examples, NumPy correspondence, inventory entries, and i-rate/k-rate regression coverage for the new selection APIs

## [0.1.3] - 2026-09-15

- Make axis handling uniform across the API: explicit negative axes now use NumPy indexing (`-1` is always the last axis), while omission is represented by a distinct overload with an operation-specific `flat`, `all axes`, or `last axis` default; k-rate overloads place the trigger before an explicit optional axis

## [0.1.2] - 2026-09-14

- Add matrix solving, inversion and determinant on a shared LU decomposition with partial pivoting, real and complex (*csnsolve*, *csninv*, *csndet*)
- Reject singular matrices in *csninv* and *csnsolve* against a pivot threshold scaled to the matrix; *csndet* answers zero, which is what the determinant of a singular matrix is
- Add the Savitzky-Golay coefficient matrix, every derivative order at once, applied with *csncorrelate1d* (*csnsavgol*)
- Add per-note and orchestra-wide real-time marking (*csnrtlockstart*, *csnrtlockend*, *csnrtlockall*), the section between start and end scoped to the note that opened it and closed when that note ends
- Run the moving median on a sliding sorted window instead of sorting every window, and drop the source copy from its in-place form (*csnmovmedian*)
- Add the median filter with scipy's zero-padded edges, along one axis on the same sliding window as the moving median, and over a box of one size per axis (*csnmedfilt1d*, *csnmedfilt*); the kernel is init-time on every overload, so a performance pass allocates nothing
- Fix in-place growth on a real-time path: *csnpush*, *csninsert*, *csnsetinsert*, *csnpad* and *csnresize* reallocated a marked array because the capacity check read a flag registry arrays never carry; the in-place block insert and pad also reallocated the source on every pass
- Fix working buffers growing on a real-time path: they are now reserved at init from the capacity of the array they serve and refused at perf time when that array is marked (moving statistics, median, norm, percentile and quantile, unique, compress, resample, sort, set operations, solve, inverse, determinant, k-rate *csntoarray* and *csnshape*)
- Fix the FFT convolutions on a path marked only through their output (*csnrtlockstart*, *csnrtlockall* or *csnrtlock* on the result): the transform buffers now follow that mark, and a new transform size is refused before *RealFFTSetup* allocates
- Fix a double free in the k-rate N-D FFT convolution, whose inverse work buffer was left aliasing the kernel's, and a wrong allocation check in the 1-D form
- Fix k-rate *csnpad* on a real-time path: the output now starts at the padded shape when the widths are known at init, so passes that keep them need no new storage; it started from the source's shape and was refused on the first pass
- Fix the init-time placeholder of the k-rate *csnpercentile* and *csnquantile* along an axis, which copied the source's element count in bytes rather than elements
- Keep a k-rate output's storage across shape changes that fit it: a producer now takes new storage only when the requested shape outgrows the room it has, twice its initial element count, instead of on every shape change. A marked output can shrink, grow back or change its count with the data (*csncompress*, *csnselect*, *csnunique*, the set operations) on a real-time path; the reused region is cleared, as fresh storage was
- Fix a hang when the k-rate convolutions (*csnconvolve1d*, *csnconvolve*, *csnfftconvolve1d*, *csnfftconvolve*), *csnsolve*, *csninv* or *csndet* raised a performance error, such as an operand freed mid-note: the error was reported with the registry lock held, and the note's deinit then waited on that lock forever
- Add the discrete cosine and sine transforms of types I and II along one axis, each carried by a real FFT over a symmetric extension rather than by the direct sum (*csndctone1d*, *csndcttwo1d*, *csndstone1d*, *csndsttwo1d*); they take their length from the array and accept any of them, since the extension is even whatever the length is, and they run about 1.5x faster than Csound's own *dct*, which extends to four times the length where these extend to two
- Add mel-frequency cepstral coefficients in one opcode, from the STFT through the filterbank and the log to the cepstral transform (*csnmfcc*); the cepstral stage is an explicit coefficients-by-bands table built at init rather than a call into the generic DCT, which at filterbank sizes is both the cheaper transform and the one that places no arithmetic condition on the band count
- Add the triangular mel filterbank as a matrix, linear or logarithmic (*csnmfbank*, *csnmlogfbank*); bands are rows, since adjacent mel bands always overlap and cannot share one
- Build the filterbank triangles at each bin's own centre frequency instead of from rounded bin edges: a band narrower than the bin spacing keeps fractional weight from its neighbours instead of collapsing to silence, which at 128 bands over a 1024-point spectrum was emptying twenty of them
- Cut the FFT copy overhead: a slice already contiguous in the item type the transform wants now moves in one block instead of one strided element at a time, and the scratch is cleared only where zero-padding actually reaches (*csnfft*, *csnrfft*, *csnifft*, *csnirfft* and the 2-D forms). Against Csound's own *rfft* the gap closes from 16-50% to 0-3%
- Add the analytic signal in one and two dimensions and the Hilbert transform on its own (*csnhilbert1d*, *csnhilbert1dr*, *csnhilbert2*); as in scipy the one named for the transform returns the analytic signal, whose real part is the source and whose imaginary part is the transform, and *csnhilbert1dr* answers that imaginary part directly through two real transforms instead of a real one and a complex one, about forty per cent less work
- Take the Hilbert length from the array and never pad it: the transform is global, so a padded length does not extend the answer but changes every sample of it, which is why an odd extent is refused rather than rounded up. There is no real counterpart to *csnhilbert2*, the product mask in two dimensions no longer leaving the real part equal to the source
- Add the Hilbert matrix (*csnhilbertmat*), which shares a name with those and nothing else: 1 / (i + j + 1), the textbook ill-conditioned example, for exercising *csnsolve*, *csninv* and *csndet* where a system is barely solvable
- Add opcode reference pages, runnable examples, and regression coverage for the new APIs

## [0.1.1] - 2026-09-09

- Add direct convolution and cross-correlation over NumPy's FULL / SAME / VALID spans, with a 1-D kernel flat or along one axis and with a kernel shaped like the source (*csnconvolve1d*, *csncorrelate1d*, *csnconvolve*, *csncorrelate*)
- Add the Fourier-transform counterparts of the same four, for the kernels long enough to make the transform pay (*csnfftconvolve1d*, *csnfftcorrelate1d*, *csnfftconvolve*, *csnfftcorrelate*)
- Extend the real-time lock to buffers an opcode owns outside the registry, so a transform on a marked path refuses to reallocate at perf time as the direct forms do
- Add opcode reference pages, runnable examples, and regression coverage for the new APIs

## [0.1.0] - 2026-09-07

- Add normalized set arrays and the complete set API (*csnlikeset*, *csnunlikeset*, *csnsetinsert*, *csnsetremove*, *csnsetcontains*, *csnsetunion*, *csnsetintersect*, *csnsetdiff*, *csnsetsymdiff*, *csnsetissubset*, *csnsetissuperset*, *csnsetisdisjoint*, *csnsetisequal*)
- Add one- and two-dimensional Fourier transforms, STFT/ISTFT, frequency-coordinate arrays, and FFT shift operations (*csnfft*, *csnrfft*, *csnifft*, *csnirfft*, *csnfft2*, *csnrfft2*, *csnifft2*, *csnirfft2*, *csnstft*, *csnistft*, *csnfftfreq*, *csnrfftfreq*, *csnfftshift*, *csnifftshift*)
- Add explicit real-time unlocking with *csnrtunlock* and triggered k-rate forms for *csnrtlock* and *csnrtunlock*
- Fix k-rate Fourier output versioning and dynamic layouts, ISTFT overlap-add accumulation, and real-time lock propagation
- Add opcode reference pages, runnable examples, and regression coverage for the new APIs

## [0.0.4] - 2026-09-04

- Add random integer generation and in-place shuffling (*csnrandint*, *csnshuffle*)
- Add 2-D row and column extraction (*csngetrow*, *csngetcol*)
- Add stacking of two or more equal-shaped arrays along a new axis, including dynamic k-rate axis support (*csnstack*)
- Fix real-time lock propagation for k-rate derived arrays (*csninterp*, *csnresample*, *csncompress*, *csnselect*)

## [0.0.3] - 2026-09-03

- Add masking and selection operations (*csnwhere*, *csnputmask*, *csncompress*, *csnselect*)
- Add element-classification masks (*csnisnan*, *csnisinf*, *csnisfin*)
- Add element-wise operations (*csnminimum*, *csnmaximum*, *csnatan2*)
- Add reduction operations (*csnrms*)
- Update existing operations with version-aware checks to avoid unnecessary recomputation
- Fixed a bug that could prevent an array safe version check

## [0.0.2] - 2026-09-02

- Add audio bridge opcodes (*csnfromaudio*, *csntoaudio*, *csnpack*, *csnunpack*, *csnsnap*, *csnstream*)
- Add perf-time guard opcode (*csnrtlock*)

## [0.0.1] - 2026-08-30

- *First release.*
