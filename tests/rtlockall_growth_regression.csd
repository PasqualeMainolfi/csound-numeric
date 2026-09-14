<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* csnrtlockall marks every array activated after it, by any note, for the
   whole performance, so it needs a file of its own. Under it an in-place
   push past capacity has to be refused, while a moving median whose window
   grows mid-note, on arrays marked from their first moment, has to run: its
   buffer was reserved at init for the widest window the source allows.

   Scored on the final marker, as the refusal counts as a failed assertion. */

csnrtlockall

giFour[]    = fillarray(1, 2, 3, 4)
giSixteen[] = fillarray(1, 5, 2, 4, 3, 9, 7, 8, 6, 0, 11, 13, 12, 10, 15, 14)

AllPush@global:CsnArr   = csnfromarray(giFour)
AllMedSrc@global:CsnArr = csnfromarray(giSixteen)
AllMedIn@global:CsnArr  = csnfromarray(giSixteen)

gkPushRefused init 0
gkMedRan      init 0
gkMedInRan    init 0

instr 1
    kOne init 1
    kValue = timeinstk()
    csnpush AllPush, kValue, kOne
    if timeinstk() == 12 then
        gkPushRefused = 1
    endif
endin

instr 2
    kOne init 1
    kAll init -1
    kWin = timeinstk() < 3 ? 2 : 9
    MedOut:CsnArr = csnmovmedian(AllMedSrc, kWin, kOne)
    if timeinstk() == 12 then
        gkMedRan = 1
    endif
endin

instr 3
    kOne init 1
    kAll init -1
    kWin = timeinstk() < 3 ? 2 : 9
    csnmovmedian AllMedIn, kWin, kOne
    if timeinstk() == 12 then
        gkMedInRan = 1
    endif
endin

instr 100
    iPush  = i(gkPushRefused)
    iMed   = i(gkMedRan)
    iMedIn = i(gkMedInRan)
    assert(iPush == 0)
    assert(iMed == 1)
    assert(iMedIn == 1)
    if iPush == 0 && iMed == 1 && iMedIn == 1 then
        prints("csnum growth guards held under csnrtlockall\n")
    endif
endin
</CsInstruments>

<CsScore>
i 1 0.0 0.02
i 2 0.1 0.02
i 3 0.2 0.02
i 100 0.3 0.01
e
</CsScore>
</CsoundSynthesizer>
