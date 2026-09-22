<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* The performance forms of the ambisonics coordinate pair and of the rotation
   matrix constructors. The i-time suite already pins the arithmetic; what only
   the k-rate forms can go wrong at is here: the trigger holding a result, each
   angle driving its own factor, the output slot being resolved again on every
   pass, and the data version moving so that a consumer downstream recomputes.

   Each note accumulates what it saw into a global, and the assertions read
   those globals at i-time in instrument 10, which the score starts after the
   notes have ended. A k-rate assert would not do: its init pass is evaluated
   once at i-time, before any control period has run. */

gkHoaX init 0
gkHoaY init 0
gkCarD init 0
gkCarEl init 0
gkRotM01 init 0
gkAxM01 init 0
gkYprM02 init 0
gkYprM12 init 0
gkSeenSum init 0

instr 1
    /* csnhoatocar follows its angles: azimuth 0 answers the front, azimuth
       pi/2 the left, so x sums to 1 and y sums to 1 over the two passes. */
    kaz init 0
    kn  init 0

    kx, ky, kz csnhoatocar 1, kaz, 0

    gkHoaX += kx
    gkHoaY += ky
    if kn == 0 then
        kaz = 1.5707963267948966
    endif
    kn += 1
    if kn == 2 then
        turnoff
    endif
endin

instr 2
    /* csncartohoa the other way, with the angle unit an i-argument even here:
       (0,0,1) is straight up, so the elevation is 90 degrees and the distance
       1 on both passes. */
    kn init 0

    kd, ka, kel csncartohoa 0, 0, 1, 1

    gkCarD  += kd
    gkCarEl += kel
    kn += 1
    if kn == 2 then
        turnoff
    endif
endin

instr 3
    /* The trigger holds the matrix. The angle reaches pi/2 on pass 1 and goes
       back to 0 on pass 2 with the trigger down, so m01 reads 0, -1, -1, -1
       and sums to -3. Without the trigger being honoured it would sum to -1. */
    kang  init 0
    ktrig init 1
    kn    init 0

    R:CsnArr = csnrotmat(kang, 2, 0, ktrig)
    Rf:CsnArr = csnflatten(R)
    kv[] = csntoarray(Rf)

    gkRotM01 += kv[1]

    if kn == 0 then
        kang = 1.5707963267948966
    elseif kn == 1 then
        ktrig = 0
        kang = 0
    endif
    kn += 1
    if kn == 4 then
        turnoff
    endif
endin

instr 4
    /* The arbitrary-axis form re-reads its direction every pass. About +z by
       pi/2 it is the same matrix csnrotmat builds for axis 2, so m01 is -1 on
       both passes and sums to -2. */
    iaxis[] = fillarray(0, 0, 1)
    axis:CsnArr = csnfromarray(iaxis)
    kang init 1.5707963267948966
    kn   init 0

    R:CsnArr = csnrotmat(axis, kang)
    Rf:CsnArr = csnflatten(R)
    kv[] = csntoarray(Rf)

    gkAxM01 += kv[1]
    kn += 1
    if kn == 2 then
        turnoff
    endif
endin

instr 5
    /* Each angle has to drive its own factor. Pitch alone lifts the front, so
       m02 is -1 on that pass; roll alone takes the left up, so m12 is -1 on
       the next. An opcode building all three matrices from the yaw would leave
       both at 0 here, since the yaw stays 0 throughout. */
    kyaw   init 0
    kpitch init 0
    kroll  init 0
    kn     init 0

    R:CsnArr = csnrotmatypr(kyaw, kpitch, kroll, 1)
    Rf:CsnArr = csnflatten(R)
    kv[] = csntoarray(Rf)

    gkYprM02 += kv[2]
    gkYprM12 += kv[5]

    if kn == 0 then
        kpitch = 90
    elseif kn == 1 then
        kpitch = 0
        kroll = 90
    endif
    kn += 1
    if kn == 3 then
        turnoff
    endif
endin

instr 6
    /* A consumer downstream has to see the matrix change. The angle is left at
       0 through the init, so the matrix starts as the identity and csnsum
       caches its sum, 3, on the pass where the trigger is still down. The
       rotation lands on pass 1, and only a data version that moved makes
       csnsum walk the array again: Rz(90) sums to 1, so the two passes add up
       to 4, where a version held still would give 6.

       The angle is raised with a k-rate assignment rather than with init, on
       purpose: an init would have been read by the init pass and the matrix
       would have started rotated, leaving nothing for the version to prove. */
    kang  init 0
    ktrig init 0
    kon   init 1
    kn    init 0

    R:CsnArr = csnrotmat(kang, 2, 0, ktrig)
    ksum:k = csnsum(R, kon)

    gkSeenSum += ksum

    if kn == 0 then
        ktrig = 1
        kang = 1.5707963267948966
    endif
    kn += 1
    if kn == 2 then
        turnoff
    endif
endin

instr 10
    iHoaX = i(gkHoaX)
    iHoaY = i(gkHoaY)
    assert(abs(iHoaX - 1) < 1e-9 && abs(iHoaY - 1) < 1e-9)

    iCarD  = i(gkCarD)
    iCarEl = i(gkCarEl)
    assert(abs(iCarD - 2) < 1e-9 && abs(iCarEl - 180) < 1e-9)

    iRotM01 = i(gkRotM01)
    assert(abs(iRotM01 + 3) < 1e-9)

    iAxM01 = i(gkAxM01)
    assert(abs(iAxM01 + 2) < 1e-9)

    iYprM02 = i(gkYprM02)
    iYprM12 = i(gkYprM12)
    assert(abs(iYprM02 + 1) < 1e-9 && abs(iYprM12 + 1) < 1e-9)

    iSeenSum = i(gkSeenSum)
    assert(abs(iSeenSum - 4) < 1e-9)
endin
</CsInstruments>

<CsScore>
i1  0   0.1
i2  0   0.1
i3  0   0.1
i4  0   0.1
i5  0   0.1
i6  0   0.1
i10 0.2 0
</CsScore>
</CsoundSynthesizer>
