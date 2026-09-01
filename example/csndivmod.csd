<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csndivmod.csd
;
; Floor division, Python-style: -7 divmod 3 is -3 remainder 2, so the remainder
; is always usable as a wrapped index.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr  = csnfromarray(array(7, -7, 10))

    quot:CsnArr, rem:CsnArr = csndivmod(data, 3)
    quot_out:i[] = csntoarray(quot)
    rem_out:i[]  = csntoarray(rem)
    prints("quotient  = %g %g %g\n", quot_out[0], quot_out[1], quot_out[2])
    prints("remainder = %g %g %g\n", rem_out[0], rem_out[1], rem_out[2])

    ; the remainder wraps an index into range whatever the sign
    idx:CsnArr   = csnfromarray(array(-2, -1, 0, 5, 9))
    q2:CsnArr, wrapped:CsnArr = csndivmod(idx, 4)
    wrapped_out:i[] = csntoarray(wrapped)
    prints("wrapped   = %g %g %g %g %g\n", wrapped_out[0], wrapped_out[1], wrapped_out[2], wrapped_out[3], wrapped_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
