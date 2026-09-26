# csnlptolpzpk

## Abstract

Moves the cutoff of a lowpass prototype, in zeros-poles-gain form.

## Description

`csnlptolpzpk` turns an analog lowpass prototype with a cutoff of 1 rad/s
into a lowpass with a cutoff of `w0` rad/s, working on its zeros, poles and
gain as `scipy.signal.lp2lp_zpk` does:

```
z' = w0 * z        p' = w0 * p        k' = k * w0^(np - nz)
```

where `nz` and `np` count the zeros and the poles. The filter keeps its
order and its number of zeros.

The zeros and poles may be real or complex and come back complex. The
prototype's zeros may be an empty array, as a Butterworth or Chebyshev type I
prototype has none.

There is no performance-time form.

## Syntax

```csound
zeros:CsnArr, poles:CsnArr, gain:i csnlptolpzpk z:CsnArr, p:CsnArr, k:i, w0:i
```

## Arguments

* `z:CsnArr`: a 1-D real or complex array of the prototype's zeros, possibly empty.
* `p:CsnArr`: a 1-D real or complex array of its poles, at least as many as the zeros.
* `k:i`: the prototype's gain.
* `w0:i`: the new cutoff in rad/s, finite and greater than zero.

## Output

* `zeros:CsnArr`: a complex vector of `nz` zeros.
* `poles:CsnArr`: a complex vector of `np` poles.
* `gain:i`: the new gain.

## Execution Time

* Init

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnlptolpzpk.csd
;
; Moves the cutoff of a lowpass prototype, in zeros-poles-gain form.
; Same arguments and results as scipy.signal.lp2lp_zpk: the zeros and poles come
; back complex, the gain as a scalar.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; analog prototype: one zero at -3, poles at -1, -2, -4, gain 2
    iz[] = fillarray(-3)
    ip[] = fillarray(-1, -2, -4)
    z:CsnArr = csnfromarray(iz)
    p:CsnArr = csnfromarray(ip)

    ; move the cutoff to 3 rad/s
    zl:CsnArr, pl:CsnArr, igain csnlptolpzpk z, p, 2, 3
    csnprint(zl)
    csnprint(pl)
    prints("gain = %.6f\n", igain)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnlptohpzpk](csnlptohpzpk.md)
* [csnlptobpzpk](csnlptobpzpk.md)
* [csnlptobszpk](csnlptobszpk.md)
* [csnbilinearzpk](csnbilinearzpk.md)
* [csnroots](csnroots.md)

## Credits

Pasquale Mainolfi, 2026
