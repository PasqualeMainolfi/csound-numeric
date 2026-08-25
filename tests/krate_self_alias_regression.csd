<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* Assigning a k-rate result back to its own input handle makes the source and
   the output the same slot. The forms that only rewrite a layout stay legal and
   must keep their data; the forms that read one array to fill another are
   rejected by CHECK_SELF_ALIAS (see krate_self_alias_rejected.csd). */

giValues[] = array(1, 2, 3, 4, 5, 6)
giShape[] = array(2, 3)

Flat@global:CsnArr = csnfromarray(giValues)
Reshaped@global:CsnArr = csnreshape(Flat, giShape)
Flattened@global:CsnArr = csnreshape(Flat, giShape)
Filled@global:CsnArr = csnlike(Reshaped, 0)

instr 1
    kShape[] = fillarray(3, 2)
    kValue init 7

    Reshaped = csnreshape(Reshaped, kShape)
    Flattened = csnflatten(Flattened)
    Filled = csnlike(Filled, kValue)
endin

instr 2
    i00[] = array(0, 0)
    i21[] = array(2, 1)
    iLast[] = array(5)

    iReshapedShape[] = csnshape(Reshaped)
    iFlatShape[] = csnshape(Flattened)
    iFilledShape[] = csnshape(Filled)

    iReshapedNdim = lenarray(iReshapedShape)
    iFlatNdim = lenarray(iFlatShape)
    iFilledNdim = lenarray(iFilledShape)

    iReshapedFirst = csnget(Reshaped, i00)
    iReshapedLast = csnget(Reshaped, i21)
    iFlatLast = csnget(Flattened, iLast)
    iFilledFirst = csnget(Filled, i00)

    /* Repeated self-assignment must not drift: the layout is idempotent and
       the data survives. */
    assert(iReshapedNdim == 2 && iReshapedShape[0] == 3 && iReshapedShape[1] == 2)
    assert(iReshapedFirst == 1 && iReshapedLast == 6)
    assert(csnsize(Reshaped) == 6)

    assert(iFlatNdim == 1 && iFlatShape[0] == 6)
    assert(iFlatLast == 6)
    assert(csnsize(Flattened) == 6)

    assert(iFilledNdim == 2 && iFilledShape[0] == 2 && iFilledShape[1] == 3)
    assert(iFilledFirst == 7)
    assert(csnsize(Filled) == 6)
endin
</CsInstruments>

<CsScore>
i 1 0 0.006
i 2 0.004 0.001
e
</CsScore>
</CsoundSynthesizer>
