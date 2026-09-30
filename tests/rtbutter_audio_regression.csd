<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

gkMaxDiff init 0
gkMaxAbs init 0
gkSwitchDiff init 0

instr 1
    iOrder = p4
    iType = p5
    iCutoff = p6
    Sos:CsnArr = csnbuttersos(iOrder, iCutoff, iType, sr)
    ain mpulse 1, 0
    kCutoff init iCutoff
    aReference = csnsosfilter(Sos, ain)
    aRealtime = csnrtbutter(ain, iOrder, kCutoff, iType, sr)

    kSample = 0
    while kSample < ksmps do
        kExpected = vaget(kSample, aReference)
        kActual = vaget(kSample, aRealtime)
        gkMaxDiff = max(gkMaxDiff, abs(kActual - kExpected))
        kSample += 1
    od
endin

instr 2
    LowSos:CsnArr = csnbuttersos(3, 500, 0, sr)
    ain oscili 0.25, 4000
    kCutoff = timeinstk() < 3 ? 500 : 4000
    aLow = csnsosfilter(LowSos, ain)
    aRealtime = csnrtbutter(ain, 3, kCutoff, 0, sr)
    kSample = 0
    while kSample < ksmps do
        kActual = vaget(kSample, aRealtime)
        gkMaxAbs = max(gkMaxAbs, abs(kActual))
        if timeinstk() > 4 then
            kLow = vaget(kSample, aLow)
            gkSwitchDiff = max(gkSwitchDiff, abs(kActual - kLow))
        endif
        kSample += 1
    od
endin

instr 99
    iMaxDiff = i(gkMaxDiff)
    iMaxAbs = i(gkMaxAbs)
    iSwitchDiff = i(gkSwitchDiff)
    assert(iMaxDiff < 1e-10)
    assert(iMaxAbs < 10)
    assert(iSwitchDiff > 0.01)
endin
</CsInstruments>
<CsScore>
i 1 0 0.004 1 0 1000
i 1 0.005 0.004 2 1 1000
i 1 0.010 0.004 3 0 4000
i 1 0.015 0.004 8 1 4000
i 2 0.020 0.020
i 99 0.041 0.001
e
</CsScore>
</CsoundSynthesizer>
