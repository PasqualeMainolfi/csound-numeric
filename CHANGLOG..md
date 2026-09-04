# Changelog

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
