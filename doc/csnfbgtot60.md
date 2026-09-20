# csnfbgtot60

## Abstract

The time a comb filter of a given delay takes to decay by 60 dB at a given feedback gain.

## Description

`csnfbgtot60` answers the decay time of a recirculating delay from its gain:

```
t60 = -3 * d / log10(g)
```

the inverse of [csnt60tofbg](csnt60tofbg.md). Where that one tunes a reverb from
a target, this one reads back what a bank you already have is really doing —
which is the measurement to take when a hand-tuned network rings longer or
shorter than it should, or when the lines of a bank turn out not to decay
together.

Only a gain strictly inside `(0, 1)` decays. At `1` the loop sustains for ever
and the logarithm is zero; above `1` it grows and there is no decay to measure.
Both answer `0` rather than an infinity or a negative time, and so does a gain
of zero or less. Every overload agrees on that, the scalar ones included.

Note how unevenly the time responds near the top of the range: at a 10 ms delay
a gain of 0.9 gives 0.66 s while 0.99 gives 6.87 s. That tenfold jump over the
last hundredth is why feedback gains are awkward to set by ear, and why tuning
through [csnt60tofbg](csnt60tofbg.md) is usually the better way round.

Four overloads cover the four ways the operands can arrive. Both scalar gives a
scalar. One scalar and one array gives a one-dimensional array of that array's
length. Both arrays gives a two-dimensional result of `delays x gains`, every delay
paired with every gain — the row index follows the delay, the column index the
gain.

The k-rate overloads follow the same four shapes. The scalar/scalar one carries
no trigger, being two operations; the three that publish a handle take a
trailing trigger, and a zero trigger republishes the previous result.

Real only.

## Syntax

```csound
value:i = csnfbgtot60(delay:i, gain:i)
handle:CsnArr = csnfbgtot60(delay:i, gains:CsnArr)
handle:CsnArr = csnfbgtot60(delays:CsnArr, gain:i)
handle:CsnArr = csnfbgtot60(delays:CsnArr, gains:CsnArr)
value:k = csnfbgtot60(delay:k, gain:k)
handle:CsnArr = csnfbgtot60(delay:k, gains:CsnArr, trig:k)
handle:CsnArr = csnfbgtot60(delays:CsnArr, gain:k, trig:k)
handle:CsnArr = csnfbgtot60(delays:CsnArr, gains:CsnArr, trig:k)
```

## Arguments

* `delay:i / delay:k`, `delays:CsnArr`: the delay length in seconds, one value or a one-dimensional array of them.
* `gain:i / gain:k`, `gains:CsnArr`: the feedback gain, one value or a one-dimensional array of them. Only values in `(0, 1)` decay.
* `trig:k` (optional, default `1`): k-rate trigger on the overloads that publish a handle. A zero trigger republishes the previous result.

## Output

* `value:i / value:k`: the decay time in seconds, when both operands are scalars.
* `handle:CsnArr`: one time per element when one operand is an array; a `delays x gains` matrix when both are.

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
; csnfbgtot60.csd
;
; How long a comb filter of a given delay takes to decay by 60 dB at a given
; feedback gain: t60 = -3 d / log10(g). The inverse of csnt60tofbg, and the way
; to find out what a reverb you already have is actually doing.
; A gain of 1 or more never decays, and answers 0.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; one comb
    t60:i           = csnfbgtot60(0.010, 0.7)
    prints("10 ms, g 0.7    : %.6f s\n", t60)

    ; the same gain in a longer delay rings longer
    t60_long:i      = csnfbgtot60(0.041, 0.7)
    prints("41 ms, g 0.7    : %.6f s\n", t60_long)

    ; a gain that does not decay has no decay time
    at_one:i        = csnfbgtot60(0.010, 1)
    above:i         = csnfbgtot60(0.010, 1.5)
    prints("g 1.0 and g 1.5 : %g and %g\n", at_one, above)

    ; a bank of delays at one gain: they do not ring for the same time
    delays:CsnArr   = csnfromarray(array(0.0297, 0.0371, 0.0411, 0.0437))
    times:CsnArr    = csnfbgtot60(delays, 0.7)
    csnprint times

    ; one delay across a range of gains
    gains:CsnArr    = csnfromarray(array(0.5, 0.7, 0.9, 0.99))
    sweep:CsnArr    = csnfbgtot60(0.010, gains)
    csnprint sweep

    ; every delay against every gain: row per delay, column per gain
    grid:CsnArr     = csnfbgtot60(delays, gains)
    csnprint grid

    ; tuning the bank to one time instead, and reading it back
    tuned:CsnArr    = csnt60tofbg(delays, 1.5)
    check:CsnArr    = csnfbgtot60(delays, tuned)
    csnprint check
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnt60tofbg](csnt60tofbg.md)
* [csnt60sab](csnt60sab.md)
* [csnt60eyr](csnt60eyr.md)
* [csnrt60absp](csnrt60absp.md)

## Credits

Pasquale Mainolfi, 2026
