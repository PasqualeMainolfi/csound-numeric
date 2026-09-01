<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnproject.csd
;
; The shadow of a on the line through b. Add csnreject to it and a comes back.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr      = csnfromarray(array(3, 4, 0))
    b:CsnArr      = csnfromarray(array(1, 0, 0))

    along:CsnArr  = csnproject(a, b)
    along_out:i[] = csntoarray(along)
    prints("along b = %g %g %g\n", along_out[0], along_out[1], along_out[2])

    across:CsnArr = csnreject(a, b)
    across_out:i[] = csntoarray(across)
    prints("across  = %g %g %g\n", across_out[0], across_out[1], across_out[2])

    ; the two halves add back up to a
    back:CsnArr   = csnadd(along, across)
    back_out:i[]  = csntoarray(back)
    prints("sum     = %g %g %g\n", back_out[0], back_out[1], back_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
