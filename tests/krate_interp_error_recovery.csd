<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

; A k-rate failure inside csninterp/csnresample happens with the registry
; mutex held, and the mutex is not recursive: an early return that skips the
; unlock wedges every later csnum call instead of just aborting the note.
; Both opcodes must report, abort their own note and leave the registry usable
; — the marker at the end only prints if they did.

Xd@global:CsnArr = csnfromarray(array(0, 1, 2, 3))
Yd@global:CsnArr = csnfromarray(array(0, 10, 20, 30))
OutOfRange@global:CsnArr = csnfromarray(array(0.5, 99))
Ramp@global:CsnArr = csnfromarray(array(0, 10, 20, 30))
Wedged@global:CsnArr = csnempty(array(0))

gkZeroLength init 0

; bounds = 0 is the "error" policy and 99 is past the last breakpoint.
instr 1
    Wedged = csninterp(OutOfRange, Xd, Yd, 0, 0)
endin

; A resample length of zero is rejected at perf time.
instr 2
    Wedged = csnresample(Ramp, gkZeroLength, 0, 1)
endin

; Anything reaching the registry after the two failures.
instr 10
    iIndex[] = fillarray(2)
    iValue = csnget(Xd, iIndex)
    assert(iValue == 2)

    iResampled:CsnArr = csnresample(Ramp, 2, 0, 1)
    iResampledValues[] = csntoarray(iResampled)
    assert(iResampledValues[0] == 0 && iResampledValues[1] == 30)

    prints "csnum registry alive after the interp and resample failures\n"
endin
</CsInstruments>

<CsScore>
i 1 0.000 0.010
i 2 0.004 0.010
i 10 0.012 0.001
e
</CsScore>
</CsoundSynthesizer>
