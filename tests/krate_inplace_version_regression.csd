<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* The in-place relayout opcodes are not idempotent: flipping an array that is
   already flipped puts it back. They carry no trigger argument, so they run on
   every k-pass, and the only thing standing between them and a permanent
   flip-flop is the guard that decides whether anything wrote the array since
   the opcode last published into it. That guard reads the array's data version
   instead of comparing the whole payload; these notes pin its behaviour to one
   relayout per write, and none without a write.

   The k-rate work runs in the early instruments and the assertions run at
   i-time in instrument 10, which the score starts after they have all ended. */

giSource[] = fillarray(1, 2, 3, 4)
giGrid[] = fillarray(1, 2, 3, 4, 5, 6)
giGridShape[] = fillarray(2, 3)

Flip@global:CsnArr = csnfromarray(giSource)
Held@global:CsnArr = csnfromarray(giSource)
Written@global:CsnArr = csnfromarray(giSource)
Rolled@global:CsnArr = csnfromarray(giSource)
GridFlat@global:CsnArr = csnfromarray(giGrid)
Grid@global:CsnArr = csnreshape(GridFlat, giGridShape)

instr 1
    /* The init pass flips once; every k-pass after it must leave the result
       alone, because nothing else writes the array. */
    kAxis init -1
    csnflip Flip, kAxis
endin

instr 2
    /* A csnset storing the value already at that index is not a write. If it
       counted as one, the array would be flipped again on the next pass, and
       again on the one after that. */
    kAxis init -1
    kIndex0[] = fillarray(0)
    csnflip Held, kAxis
    csnset Held, kIndex0, 4
endin

instr 3
    /* A real write has to be seen exactly once. Instrument 6 starts later and
       writes 9 over index 0 on a single i-time pass; the flip that follows
       turns 9 3 2 1 into 1 2 3 9 and then holds, with nothing writing again.

       The write lives in its own instrument on purpose. An opcode written
       inside a k-rate `if` still runs its init pass at i-time — the branch only
       gates the performance pass — so a csnset placed here would have written
       before the first k-pass and made the note untestable. */
    kAxis init -1
    csnflip Written, kAxis
endin

instr 4
    /* csnroll.in with an unchanging shift: one roll at init, nothing after. */
    kShift init 1
    csnroll Rolled, kShift
endin

instr 5
    /* csntranspose.in rewrites shape as well as data, so its shape version
       moves with its data version; the guard still has to hold it steady. */
    kAxes[] = fillarray(1, 0)
    csntranspose Grid, kAxes
endin

instr 6
    kIndex0[] = fillarray(0)
    csnset Written, kIndex0, 9
endin

instr 10
    i0[] = array(0)
    i1[] = array(1)
    i2[] = array(2)
    i3[] = array(3)

    iFlip0 = csnget(Flip, i0)
    iFlip1 = csnget(Flip, i1)
    iFlip2 = csnget(Flip, i2)
    iFlip3 = csnget(Flip, i3)
    assert(iFlip0 == 4 && iFlip1 == 3 && iFlip2 == 2 && iFlip3 == 1)

    iHeld0 = csnget(Held, i0)
    iHeld1 = csnget(Held, i1)
    iHeld2 = csnget(Held, i2)
    iHeld3 = csnget(Held, i3)
    assert(iHeld0 == 4 && iHeld1 == 3 && iHeld2 == 2 && iHeld3 == 1)

    iWritten0 = csnget(Written, i0)
    iWritten1 = csnget(Written, i1)
    iWritten2 = csnget(Written, i2)
    iWritten3 = csnget(Written, i3)
    assert(iWritten0 == 1 && iWritten1 == 2 && iWritten2 == 3 && iWritten3 == 9)

    iRolled0 = csnget(Rolled, i0)
    iRolled1 = csnget(Rolled, i1)
    iRolled2 = csnget(Rolled, i2)
    iRolled3 = csnget(Rolled, i3)
    assert(iRolled0 == 4 && iRolled1 == 1 && iRolled2 == 2 && iRolled3 == 3)

    iGridShape[] = csnshape(Grid)
    assert(iGridShape[0] == 3 && iGridShape[1] == 2)

    i00[] = array(0, 0)
    i01[] = array(0, 1)
    i10[] = array(1, 0)
    i21[] = array(2, 1)
    iGrid00 = csnget(Grid, i00)
    iGrid01 = csnget(Grid, i01)
    iGrid10 = csnget(Grid, i10)
    iGrid21 = csnget(Grid, i21)
    assert(iGrid00 == 1 && iGrid01 == 4 && iGrid10 == 2 && iGrid21 == 6)
endin
</CsInstruments>

<CsScore>
i1  0    0.1
i2  0    0.1
i3  0    0.1
i4  0    0.1
i5  0    0.1
i6  0.05 0
i10 0.2  0
</CsScore>
</CsoundSynthesizer>
