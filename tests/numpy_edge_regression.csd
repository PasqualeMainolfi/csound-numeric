<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* Edge cases of the dense reduction, clip and moving-window paths, checked
   against what numpy 2 returns for the same input. */

instr 1
    iInf = 1 / 0
    iNaN = iInf - iInf

    ; min/max keep an infinite extreme (a DBL_MAX seed used to replace it)
    iOnlyInf[] = fillarray(iInf)
    OnlyInf:CsnArr = csnfromarray(iOnlyInf)
    iMinInf = csnmin(OnlyInf)
    assert(iMinInf == iInf)
    iNegInf[] = fillarray(-iInf, -iInf)
    NegInf:CsnArr = csnfromarray(iNegInf)
    iMaxNegInf = csnmax(NegInf)
    assert(iMaxNegInf == -iInf)

    ; a NaN anywhere makes min and max NaN
    iWithNaN[] = fillarray(1, 2, iNaN, 4)
    WithNaN:CsnArr = csnfromarray(iWithNaN)
    iMinNaN = csnmin(WithNaN)
    assert(iMinNaN != iMinNaN)

    ; np.var / np.std, two-pass: [1e8+1 .. 1e8+5] -> 2 and sqrt(2)
    iBig[] = fillarray(1e8 + 1, 1e8 + 2, 1e8 + 3, 1e8 + 4, 1e8 + 5)
    Big:CsnArr = csnfromarray(iBig)
    iVar = csnvar(Big)
    iStd = csnstd(Big)
    assert(iVar == 2)
    assert(abs(iStd - sqrt(2)) < 1e-15)

    ; np.clip(x, 3, 1.5): min above max makes every value max, NaN stays
    Clipped:CsnArr = csnclip(WithNaN, 3, 1.5)
    iClip[] = csntoarray(Clipped)
    assert(iClip[0] == 1.5 && iClip[1] == 1.5 && iClip[3] == 1.5)
    assert(iClip[2] != iClip[2])

    ; a moving window holding only NaNs answers NaN, others skip the NaN
    iTwoNaN[] = fillarray(5, iNaN, iNaN, 2)
    TwoNaN:CsnArr = csnfromarray(iTwoNaN)
    MovMin:CsnArr = csnmovmin(TwoNaN, 2)
    iMov[] = csntoarray(MovMin)
    assert(iMov[0] == 5 && iMov[3] == 2)
    assert(iMov[2] != iMov[2])
endin
</CsInstruments>
<CsScore>
i 1 0 0.001
e
</CsScore>
</CsoundSynthesizer>
