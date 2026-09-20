<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnt60sab.csd
;
; Sabine's reverberation time. The absorption of a room is the sum of every
; surface times its own coefficient, and T60 is 0.161 V / A. The scalar form
; takes one room; the array form takes one volume per row and the matching row
; of surfaces and coefficients, so a whole set of rooms is answered at once.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; a 200 m3 room: 60 m2 of plaster, 40 m2 of carpet, 30 m2 of curtain
    surfaces:CsnArr = csnfromarray(array(60, 40, 30))
    alphas:CsnArr   = csnfromarray(array(0.1, 0.25, 0.6))

    ; absorption = 60*0.1 + 40*0.25 + 30*0.6 = 34 sabins
    t60:i           = csnt60sab(200, surfaces, alphas)
    prints("T60 (200 m3)    : %.4f s\n", t60)

    ; doubling the volume doubles the time: nothing absorbs any more than before
    t60_big:i       = csnt60sab(400, surfaces, alphas)
    prints("T60 (400 m3)    : %.4f s\n", t60_big)

    ; the same two rooms in one call: one volume per row, one row of surfaces
    ; and of coefficients each
    shape:i[]       = fillarray(2, 3)
    volumes:CsnArr  = csnfromarray(array(200, 400))
    surf_mat:CsnArr = csnreshape(csnfromarray(array(60, 40, 30, 60, 40, 30)), shape)
    alph_mat:CsnArr = csnreshape(csnfromarray(array(0.1, 0.25, 0.6, 0.1, 0.25, 0.6)), shape)

    times:CsnArr    = csnt60sab(volumes, surf_mat, alph_mat)
    csnprint times

    ; more absorbent curtains, shorter tail
    dead:CsnArr     = csnfromarray(array(0.1, 0.25, 0.9))
    t60_dead:i      = csnt60sab(200, surfaces, dead)
    prints("T60 (absorbent) : %.4f s\n", t60_dead)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
