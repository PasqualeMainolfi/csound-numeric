# Changelog

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
