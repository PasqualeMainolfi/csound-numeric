<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* 'create' allocates the callback's dataspace but leaves its argument cells
   unconnected: only a call such as "kout init op, kin" wires them. csnforeach
   writes an element through inargp[0] and reads the result from outargp[0], so
   an unwired callback is a null dereference rather than a wrong answer.
   Expected to raise "was created but never wired" — the ctest entry matches on
   that text, so this file is a failure case by design. */

giSource[] = fillarray(0, 1, 2, 3)

Source@global:CsnArr = csnfromarray(giSource)

opcode square_plus_one(x:k):k
    xout x * x + 1
endop

instr 1
    fn:OpcodeDef init "square_plus_one"
    op:Opcode create fn
    ktrig init 1
    csnforeach(Source, op, ktrig)
endin
</CsInstruments>

<CsScore>
i 1 0 0.003
e
</CsScore>
</CsoundSynthesizer>
