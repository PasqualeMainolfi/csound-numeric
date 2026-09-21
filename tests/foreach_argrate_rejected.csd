<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* csnforeach writes each element into the callback's argument cell as a MYFLT
   and reads the result back out. An i-rate callback accepts that write, but a
   UDO always carries a perf function, so it runs an empty performance chain
   and leaves the result cell at whatever its init pass wrote: every element
   would come back as the same value, with no error anywhere. The rate check
   turns that silence into a refusal.

   The same check guards the worse case, a constant cell, which set_constant
   shares across the whole engine.

   Expected to raise "must be k-rate" — the ctest entry matches on that text,
   so this file is a failure case by design. */

giSource[] = fillarray(0, 1, 2, 3)

Source@global:CsnArr = csnfromarray(giSource)

opcode square_plus_one_i(x:i):i
    xout x * x + 1
endop

instr 1
    fn:OpcodeDef init "square_plus_one_i"
    op:Opcode create fn
    iin init 0
    iout:i init op, iin
    ktrig init 1
    csnforeach(Source, op, ktrig)
endin
</CsInstruments>

<CsScore>
i 1 0 0.003
e
</CsScore>
</CsoundSynthesizer>
