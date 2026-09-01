<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnunique.csd
;
; Each value once, in order. The length of the result is the count of distinct
; values, which is how a pitch set is reduced to its pitch classes.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(3, 1, 4, 1, 5))

    values:CsnArr  = csnunique(data)
    values_out:i[] = csntoarray(values)
    count:i        = csnsize(values)
    prints("distinct = %d : %g %g %g %g\n", count, values_out[0], values_out[1], values_out[2], values_out[3])

    ; midi notes reduced to pitch classes
    notes:CsnArr   = csnfromarray(array(60, 64, 67, 72, 76, 79))
    q:CsnArr, pc:CsnArr = csndivmod(notes, 12)
    classes:CsnArr = csnunique(pc)
    classes_out:i[] = csntoarray(classes)
    classes_n:i    = csnsize(classes)
    prints("pitch classes = %d : %g %g %g\n", classes_n, classes_out[0], classes_out[1], classes_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
