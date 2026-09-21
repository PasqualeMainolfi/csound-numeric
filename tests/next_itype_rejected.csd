<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* csnnext has one overload per element type, chosen by the type of the value
   output, and its working copy is laid out for the type it was told to expect:
   one double per element for a real array, an interleaved pair for a complex
   one. Reading a complex array through the real form would walk the real and
   imaginary parts as if they were separate elements, so the mismatch is
   refused where the copy is taken, at init, rather than at the first
   performance pass.

   Expected to raise "csnnext real requires real array" — the ctest entry
   matches on that text, so this file is a failure case by design. */

giSource[] = fillarray(10, 20, 30)

Real@global:CsnArr = csnfromarray(giSource)
Complex@global:CsnArr = csntocomplex(Real)

instr 1
    kon init 1
    kvalue, kstate csnnext Complex, kon
endin
</CsInstruments>

<CsScore>
i 1 0 0.003
e
</CsScore>
</CsoundSynthesizer>
