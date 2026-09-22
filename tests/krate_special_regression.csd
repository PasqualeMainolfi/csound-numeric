<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* The performance forms of the Legendre functions and the spherical
   harmonics. The i-time suite already pins the arithmetic; what only the
   k-rate forms can go wrong at is here: a scalar following its arguments, the
   trigger holding a result, an elementwise form following writes to its
   source and surviving assignment back to its own input, and the direction
   matrix recomputing when a direction moves.

   Each note accumulates what it saw into a global, and the assertions read
   those globals at i-time in instrument 10, which the score starts after the
   notes have ended. Instrument 5 outlasts the check: a global handle a note
   produced reads 0 once that note is gone. */

giX[] = fillarray(0, 0.5, 1)
giAz[] = fillarray(0, 90)
giEl[] = fillarray(0, 0)

LegSrc@global:CsnArr = csnfromarray(giX)
LegOut@global:CsnArr = csnfromarray(giX)
LegSelf@global:CsnArr = csnfromarray(giX)
DirAz@global:CsnArr = csnfromarray(giAz)
DirEl@global:CsnArr = csnfromarray(giEl)
DirMat@global:CsnArr = csnfromarray(giAz)

gkLegScalar init 0
gkSphScalar init 0
gkAcnHeld init 0
gkAcnSum init 0

instr 1
    /* csnlegendre follows x: P_2^1 is 3x sqrt(1-x^2), 0 at x = 0 and
       3*0.6*0.8 = 1.44 at x = 0.6, so the two passes sum to 1.44. */
    kx init 0
    kn init 0

    kP = csnlegendre(2, 1, kx)
    gkLegScalar += kP
    kx = 0.6
    kn += 1
    if kn == 2 then
        turnoff
    endif
endin

instr 2
    /* csnsphharm follows its angles, with n and m k-rate too: Y_1^1 at
       azimuth 0 reads 1, then Y_1^-1 at azimuth 90 reads 1 again. */
    kaz init 0
    km  init 1
    kn  init 0

    kY = csnsphharm(1, km, kaz, 0, 1)
    gkSphScalar += kY
    kaz = 90
    km = -1
    kn += 1
    if kn == 2 then
        turnoff
    endif
endin

instr 3
    /* The trigger holds the set. The azimuth moves to 90 on pass 1 with the
       trigger down, so ACN 3 (X) keeps reading its init value, cos 0 = 1, on
       both passes and sums to 2; followed, it would sum to 1. */
    kaz   init 0
    ktrig init 0
    kn    init 0

    Set:CsnArr = csnsphharmacn(1, kaz, 0, 1, ktrig)
    kv[] = csntoarray(Set)
    gkAcnHeld += kv[3]
    kaz = 90
    kn += 1
    if kn == 2 then
        turnoff
    endif
endin

instr 4
    /* A consumer downstream has to see the set change. At azimuth 0 the
       order-1 set is (1, 0, 0, 1) and sums to 2; at azimuth 45 it is
       (1, 1/sqrt 2, 0, 1/sqrt 2) and sums to 1 + sqrt 2. The azimuth moves
       after the first pass, so three passes add up to 4 + 2 sqrt 2. Only a
       data version that moved makes csnsum walk the array again: held still,
       it would answer 6. */
    kaz  init 0
    kon  init 1
    kn   init 0

    Set:CsnArr = csnsphharmacn(1, kaz, 0, 1, kon)
    ksum:k = csnsum(Set, kon)
    gkAcnSum += ksum
    kaz = 45
    kn += 1
    if kn == 3 then
        turnoff
    endif
endin

instr 5
    kOne init 1
    kZero init 0
    /* Elementwise, following its source. */
    LegOut = csnlegendre(1, 0, LegSrc, kOne)
    /* Assigned back to its own input: P_1^0 is the identity, so however many
       passes run, the cells keep their values. */
    LegSelf = csnlegendre(1, 0, LegSelf, kOne)
    /* The direction matrix follows a moved direction. */
    DirMat = csnsphharmacn(1, DirAz, DirEl, 1, kOne)
endin

instr 6
    /* Moves the sources after instrument 5 has run once on the originals. */
    kIndex0[] = fillarray(0)
    csnset LegSrc, kIndex0, -0.25
    csnset DirAz, kIndex0, 180
endin

instr 10
    iLegScalar = i(gkLegScalar)
    assert(abs(iLegScalar - 1.44) < 1e-12)

    iSphScalar = i(gkSphScalar)
    assert(abs(iSphScalar - 2) < 1e-12)

    iAcnHeld = i(gkAcnHeld)
    assert(abs(iAcnHeld - 2) < 1e-12)

    iAcnSum = i(gkAcnSum)
    assert(abs(iAcnSum - (4 + 2 * sqrt(2))) < 1e-12)

    i0[] = array(0)
    i1[] = array(1)
    i2[] = array(2)
    iLeg0 = csnget(LegOut, i0)
    iLeg1 = csnget(LegOut, i1)
    assert(abs(iLeg0 + 0.25) < 1e-12 && abs(iLeg1 - 0.5) < 1e-12)

    iSelf1 = csnget(LegSelf, i1)
    iSelf2 = csnget(LegSelf, i2)
    assert(abs(iSelf1 - 0.5) < 1e-12 && abs(iSelf2 - 1) < 1e-12)

    ; ACN 3 (X) of the first direction, now at azimuth 180, reads -1; the
    ; second direction, at azimuth 90, keeps X = 0 and Y = 1
    iM30[] = array(3, 0)
    iM31[] = array(3, 1)
    iM11[] = array(1, 1)
    iX0 = csnget(DirMat, iM30)
    iX1 = csnget(DirMat, iM31)
    iY1 = csnget(DirMat, iM11)
    assert(abs(iX0 + 1) < 1e-12 && abs(iX1) < 1e-12 && abs(iY1 - 1) < 1e-12)
endin
</CsInstruments>

<CsScore>
i1  0    0.1
i2  0    0.1
i3  0    0.1
i4  0    0.1
i5  0    0.5
i6  0.05 0.01
i10 0.3  0
</CsScore>
</CsoundSynthesizer>
