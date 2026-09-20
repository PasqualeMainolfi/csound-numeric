# csnt60tofbg

## Abstract

The feedback gain a comb filter of a given delay needs to decay by 60 dB in a given time.

## Description

`csnt60tofbg` answers the gain of a recirculating delay from the decay time you
want:

```
g = 10 ^ (-3 * d / t60)
```

`d` is the delay in seconds and `t60` the time in seconds to fall by 60 dB. The
exponent is the count of round trips in that time, `t60 / d`, times the 3 that
turns a factor-of-ten-cubed attenuation into one decade per pass.

This is how a Schroeder or feedback-delay-network reverb is tuned from a single
number. Give the bank its delays — mutually prime, so the echo pattern does not
lock into a comb — hand them all the same `t60`, and every line comes back with
the gain that makes it decay at the same rate. A longer delay recirculates less
often over the same span, so it needs a gain closer to 1 to last as long; the
gains are therefore never uniform across a bank, and that is the point.

The result is always in `(0, 1)` for a positive delay and a positive time, which
is exactly the range a stable feedback loop needs.
[csnfbgtot60](csnfbgtot60.md) is the inverse.

A delay or a time of zero, or a negative one, answers `0`: there is no such
filter to describe. Every overload agrees on that, the scalar ones included.

Four overloads cover the four ways the operands can arrive. Both scalar gives a
scalar. One scalar and one array gives a one-dimensional array of that array's
length. Both arrays gives a two-dimensional result of `delays x t60s`, every delay
paired with every target time — the row index follows the delay, the column index the
time.

The k-rate overloads follow the same four shapes. The scalar/scalar one carries
no trigger, being two operations; the three that publish a handle take a
trailing trigger, and a zero trigger republishes the previous result.

Real only.

## Syntax

```csound
value:i = csnt60tofbg(delay:i, t60:i)
handle:CsnArr = csnt60tofbg(delay:i, t60s:CsnArr)
handle:CsnArr = csnt60tofbg(delays:CsnArr, t60:i)
handle:CsnArr = csnt60tofbg(delays:CsnArr, t60s:CsnArr)
value:k = csnt60tofbg(delay:k, t60:k)
handle:CsnArr = csnt60tofbg(delay:k, t60s:CsnArr, trig:k)
handle:CsnArr = csnt60tofbg(delays:CsnArr, t60:k, trig:k)
handle:CsnArr = csnt60tofbg(delays:CsnArr, t60s:CsnArr, trig:k)
```

## Arguments

* `delay:i / delay:k`, `delays:CsnArr`: the delay length in seconds, one value or a one-dimensional array of them.
* `t60:i / t60:k`, `t60s:CsnArr`: the decay time wanted in seconds, one value or a one-dimensional array of them.
* `trig:k` (optional, default `1`): k-rate trigger on the overloads that publish a handle. A zero trigger republishes the previous result.

## Output

* `value:i / value:k`: the feedback gain, when both operands are scalars.
* `handle:CsnArr`: one gain per element when one operand is an array; a `delays x t60s` matrix when both are.

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

; -----------------------------------------------------------------------------
; csnt60tofbg.csd
;
; The feedback gain a comb filter of a given delay needs in order to decay by
; 60 dB in a given time: g = 10^(-3 d / t60). A longer delay recirculates less
; often in the same time, so it needs a gain closer to 1 to last as long.
; The usual way to tune a Schroeder or FDN reverb from a single T60.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; one comb, one target
    g:i             = csnt60tofbg(0.010, 1.5)
    prints("10 ms, 1.5 s    : %.6f\n", g)

    ; the same time from a longer delay needs a gain nearer 1
    g_long:i        = csnt60tofbg(0.041, 1.5)
    prints("41 ms, 1.5 s    : %.6f\n", g_long)

    ; a bank of mutually prime delays, all tuned to one reverberation time
    delays:CsnArr   = csnfromarray(array(0.0297, 0.0371, 0.0411, 0.0437))
    gains:CsnArr    = csnt60tofbg(delays, 1.5)
    csnprint gains

    ; one delay against a range of target times
    targets:CsnArr  = csnfromarray(array(0.5, 1.5, 3.0))
    sweep:CsnArr    = csnt60tofbg(0.010, targets)
    csnprint sweep

    ; every delay against every target: row per delay, column per target
    grid:CsnArr     = csnt60tofbg(delays, targets)
    csnprint grid

    ; and back, which is what csnfbgtot60 is for. Two arrays pair every delay
    ; with every gain, so the round trip is the diagonal: 1.5 all the way down
    back:CsnArr     = csnfbgtot60(delays, gains)
    csnprint back
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnfbgtot60](csnfbgtot60.md)
* [csnt60sab](csnt60sab.md)
* [csnt60eyr](csnt60eyr.md)
* [csnrt60absp](csnrt60absp.md)

## Credits

Pasquale Mainolfi, 2026
