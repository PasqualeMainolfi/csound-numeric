<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnrt60absp.csd
;
; Sabine read backwards: given a volume and the reverberation time you want,
; how much absorption does the room need? A = 0.161 V / T60, in sabins. Either
; side may be an array; when both are, every volume is paired with every target
; and the result is a volumes x targets matrix.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; one room, one target
    absorption:i     = csnrt60absp(200, 0.8)
    prints("200 m3 -> 0.8 s : %.3f sabins\n", absorption)

    ; one room, a range of targets: a drier room needs more absorption
    targets:CsnArr   = csnfromarray(array(0.6, 0.8, 1.2, 2.0))
    needed:CsnArr    = csnrt60absp(200, targets)
    csnprint needed

    ; several rooms, one target: a bigger room needs proportionally more
    volumes:CsnArr   = csnfromarray(array(100, 200, 400))
    per_room:CsnArr  = csnrt60absp(volumes, 0.8)
    csnprint per_room

    ; both as arrays: row per volume, column per target
    grid:CsnArr      = csnrt60absp(volumes, targets)
    csnprint grid

    ; what is missing from a room that has 20 sabins today
    have:i           = 20
    short:i          = absorption - have
    prints("still missing   : %.3f sabins\n", short)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
