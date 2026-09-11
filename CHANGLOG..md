# Changelog

## [0.1.2] - 

- Add matrix solving, inversion and determinant on a shared LU decomposition with partial pivoting, real and complex (*csnsolve*, *csninv*, *csndet*)
- Reject singular matrices in *csninv* and *csnsolve* against a pivot threshold scaled to the matrix; *csndet* answers zero, which is what the determinant of a singular matrix is
- Add the Savitzky-Golay coefficient matrix, every derivative order at once, applied with *csncorrelate1d* (*csnsavgol*)
- Add per-note and orchestra-wide real-time marking (*csnrtlockstart*, *csnrtlockend*, *csnrtlockall*), the section between start and end scoped to the note that opened it and closed when that note ends
- Run the moving median on a sliding sorted window instead of sorting every window, and drop the source copy from its in-place form (*csnmovmedian*)
- Fix in-place growth on a real-time path: *csnpush*, *csninsert*, *csnsetinsert*, *csnpad* and *csnresize* reallocated a marked array because the capacity check read a flag registry arrays never carry; the in-place block insert and pad also reallocated the source on every pass
- Fix working buffers growing on a real-time path: they are now reserved at init from the capacity of the array they serve and refused at perf time when that array is marked (moving statistics, median, norm, percentile and quantile, unique, compress, resample, sort, set operations, solve, inverse, determinant, k-rate *csntoarray* and *csnshape*)
- Fix the FFT convolutions on a path marked only through their output (*csnrtlockstart*, *csnrtlockall* or *csnrtlock* on the result): the transform buffers now follow that mark, and a new transform size is refused before *RealFFTSetup* allocates
- Fix a double free in the k-rate N-D FFT convolution, whose inverse work buffer was left aliasing the kernel's, and a wrong allocation check in the 1-D form
- Fix k-rate *csnpad* on a real-time path: the output now starts at the padded shape when the widths are known at init, so passes that keep them need no new storage; it started from the source's shape and was refused on the first pass
- Fix the init-time placeholder of the k-rate *csnpercentile* and *csnquantile* along an axis, which copied the source's element count in bytes rather than elements
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
