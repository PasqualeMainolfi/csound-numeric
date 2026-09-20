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
