<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnfschrd.csd
;
; The Schroeder frequency, 2000 sqrt(T60 / V): the crossover between the modal
; region of a room, where individual resonances are separate and audible, and
; the diffuse region above it, where they overlap into a statistical field.
; Reverb design lives above it; room correction lives below.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; a small live room: the modal region reaches high
    small:i          = csnfschrd(50, 1.2)
    prints("50 m3, 1.2 s    : %.2f Hz\n", small)

    ; a concert hall: almost everything audible is already diffuse
    hall:i           = csnfschrd(12000, 2.0)
    prints("12000 m3, 2.0 s : %.2f Hz\n", hall)

    ; one room, several possible treatments
    targets:CsnArr   = csnfromarray(array(0.4, 0.8, 1.6))
    curve:CsnArr     = csnfschrd(200, targets)
    csnprint curve

    ; several rooms, one reverberation time
    volumes:CsnArr   = csnfromarray(array(50, 200, 800))
    by_room:CsnArr   = csnfschrd(volumes, 1.2)
    csnprint by_room

    ; every room against every target
    grid:CsnArr      = csnfschrd(volumes, targets)
    csnprint grid
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
