<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* csnnext walks a copy of the elements taken at init, which is what lets it
   answer without holding the registry lock. That copy going stale under it is
   the one failure the design cannot absorb, so a write to the source while the
   walk is in progress is refused instead of being served from the old values.

   The write lives in its own instrument, scheduled from inside the walk. An
   opcode written in a k-rate branch still runs its init pass at i-time — the
   branch only gates the performance pass — so a csnset placed in instrument 1
   would have written before the first element was ever read.

   Expected to raise "array is changed while iterating" — the ctest entry
   matches on that text, so this file is a failure case by design. */

giSource[] = fillarray(1, 2, 3, 4, 5, 6, 7, 8)
giIndex[] = fillarray(0)

Source@global:CsnArr = csnfromarray(giSource)

instr 1
    kon init 1
    kn  init 0

    kvalue, kstate csnnext Source, kon

    if kn == 2 then
        event "i", 2, 0, 0.003
    endif

    kn += 1
    if kn == 8 then
        turnoff
    endif
endin

instr 2
    csnset Source, giIndex, 99
    turnoff
endin
</CsInstruments>

<CsScore>
i 1 0 0.02
e
</CsScore>
</CsoundSynthesizer>
