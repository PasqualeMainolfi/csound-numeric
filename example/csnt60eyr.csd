<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnt60eyr.csd
;
; Eyring's reverberation time. Same inputs as csnt60sab, different model: the
; mean absorption coefficient enters through a logarithm, so the two agree in a
; live room and part company as the surfaces get more absorbent. Eyring is the
; one that still answers when the room absorbs nearly everything.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    surfaces:CsnArr = csnfromarray(array(60, 40, 30))
    alphas:CsnArr   = csnfromarray(array(0.1, 0.25, 0.6))

    ; S = 130 m2, A = 34 sabins, mean coefficient 0.2615
    eyring:i        = csnt60eyr(200, surfaces, alphas)
    sabine:i        = csnt60sab(200, surfaces, alphas)
    prints("Eyring          : %.4f s\n", eyring)
    prints("Sabine          : %.4f s\n", sabine)

    ; a live room, mean coefficient 0.05: the two models agree
    live:CsnArr     = csnfromarray(array(0.05, 0.05, 0.05))
    eyring_live:i   = csnt60eyr(200, surfaces, live)
    sabine_live:i   = csnt60sab(200, surfaces, live)
    prints("live  Eyring    : %.4f s\n", eyring_live)
    prints("live  Sabine    : %.4f s\n", sabine_live)

    ; a dead room, mean coefficient 0.85: Sabine still reports a tail that is
    ; not there, Eyring collapses towards zero as the logarithm demands
    dead:CsnArr     = csnfromarray(array(0.85, 0.85, 0.85))
    eyring_dead:i   = csnt60eyr(200, surfaces, dead)
    sabine_dead:i   = csnt60sab(200, surfaces, dead)
    prints("dead  Eyring    : %.4f s\n", eyring_dead)
    prints("dead  Sabine    : %.4f s\n", sabine_dead)

    ; one row per room, as in csnt60sab
    shape:i[]       = fillarray(2, 3)
    volumes:CsnArr  = csnfromarray(array(200, 400))
    surf_mat:CsnArr = csnreshape(csnfromarray(array(60, 40, 30, 60, 40, 30)), shape)
    alph_mat:CsnArr = csnreshape(csnfromarray(array(0.1, 0.25, 0.6, 0.1, 0.25, 0.6)), shape)
    times:CsnArr    = csnt60eyr(volumes, surf_mat, alph_mat)
    csnprint times
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
