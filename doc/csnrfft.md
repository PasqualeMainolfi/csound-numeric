# csnrfft

## Abstract

Compute the non-negative-frequency half of the FFT of a real array.

## Description

`csnrfft` computes an `nfft`-point FFT of real input and returns the one-sided
complex spectrum from DC through Nyquist. Its length along `axis` is
`floor(nfft / 2) + 1`; for the required power-of-two sizes this is
`nfft / 2 + 1`.

The selected source axis is zero-padded or truncated to `nfft` before the
transform. All other dimensions are preserved. Complex input is rejected.
Use [csnirfft](csnirfft.md) for the inverse.

For a Csound audio signal, first use [csnsnap](csnsnap.md) to collect frames of
the desired FFT length. This keeps `nfft` independent of `ksmps`; pass the
`kready` output from `csnsnap` as this opcode's trigger. The audio frame is
real-time marked by default and the spectrum inherits that mark.

## Syntax

```csound
spectrum:CsnArr = csnrfft(source:CsnArr, nfft:i)
spectrum:CsnArr = csnrfft(source:CsnArr, nfft:i, axis:i)
spectrum:CsnArr = csnrfft(source:CsnArr, nfft:i, trig:k)
spectrum:CsnArr = csnrfft(source:CsnArr, nfft:i, trig:k, axis:i)
```

## Arguments

* `source:CsnArr`: real input array.
* `nfft:i`: transform length, a positive power of two.
* `axis:i` (optional): transform axis. Omit it to use the last axis; negative values count from the end.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes the previous result.

## Output

* `spectrum:CsnArr`: complex array with `nfft / 2 + 1` bins on the transform axis.

## Execution Time

* Init
* Performance (k-rate)

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    aSignal oscili 0.5, 3000

    ; The analysis size is independent of ksmps. csnsnap buffers the audio and
    ; raises kReady only when a complete, 50%-overlapped frame is available.
    frame:CsnArr, kReady = csnsnap(aSignal, 256, 128)
    spectrum:CsnArr = csnrfft(frame, 256, kReady, -1)
    magnitude:CsnArr = csnabs(spectrum, kReady)
    kPeak = csnmax(magnitude, kReady)

    ; csnsnap marks frame as a real-time path by default; spectrum and magnitude
    ; inherit the mark, so no explicit csnrtlock is needed in this chain.
    printf("new spectrum: %d bins, peak magnitude %.3f\n", kReady, 129, kPeak)
endin
</CsInstruments>
<CsScore>
i 1 0 0.03
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnirfft](csnirfft.md)
* [csnfft](csnfft.md)
* [csnrfftfreq](csnrfftfreq.md)
* [csnsnap](csnsnap.md)
* [csnrtlock](csnrtlock.md)

## Credits

Pasquale Mainolfi, 2026
