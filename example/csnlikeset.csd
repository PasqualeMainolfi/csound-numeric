<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    raw:CsnArr = csnfromarray(array(3, 1, 2, 2, 1))
    values:CsnArr = csnlikeset(raw)
    out:i[] = csntoarray(values)
    prints("set = %g %g %g\n", out[0], out[1], out[2])
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
