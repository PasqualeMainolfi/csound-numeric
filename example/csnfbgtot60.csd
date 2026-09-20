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
