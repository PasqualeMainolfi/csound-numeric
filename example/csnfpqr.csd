<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnfpqr.csd
;
; The resonant frequencies of a rectangular room. For mode (p, q, r) and
; dimensions (L, W, H) the frequency is (c/2) sqrt((p/L)^2 + (q/W)^2 + (r/H)^2).
; A mode with one non-zero index is axial, two tangential, three oblique; the
; axial ones are the loudest and the ones that make a room sound uneven.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    room:CsnArr     = csnfromarray(array(7, 5, 3))

    ; the three first-order axial modes, one per dimension
    along_l:CsnArr  = csnfromarray(array(1, 0, 0))
    along_w:CsnArr  = csnfromarray(array(0, 1, 0))
    along_h:CsnArr  = csnfromarray(array(0, 0, 1))
    f_l:i           = csnfpqr(room, along_l, 343)
    f_w:i           = csnfpqr(room, along_w, 343)
    f_h:i           = csnfpqr(room, along_h, 343)
    prints("axial L (1,0,0) : %.2f Hz\n", f_l)
    prints("axial W (0,1,0) : %.2f Hz\n", f_w)
    prints("axial H (0,0,1) : %.2f Hz\n", f_h)

    ; the orders are not flags: (2,0,0) is the octave above (1,0,0)
    second:CsnArr   = csnfromarray(array(2, 0, 0))
    f_second:i      = csnfpqr(room, second, 343)
    prints("axial L (2,0,0) : %.2f Hz\n", f_second)

    ; tangential and oblique
    tang:CsnArr     = csnfromarray(array(1, 1, 0))
    obl:CsnArr      = csnfromarray(array(1, 1, 1))
    f_tang:i        = csnfpqr(room, tang, 343)
    f_obl:i         = csnfpqr(room, obl, 343)
    prints("tangential      : %.2f Hz\n", f_tang)
    prints("oblique         : %.2f Hz\n", f_obl)

    ; one row per room: the same mode across three candidate shapes
    shape:i[]       = fillarray(3, 3)
    rooms:CsnArr    = csnreshape(csnfromarray(array(7, 5, 3, 6, 5, 3, 5, 5, 3)), shape)
    modes:CsnArr    = csnfpqr(rooms, along_l, 343)
    csnprint modes

    ; a colder room slows sound down and drops every mode with it
    f_cold:i        = csnfpqr(room, along_l, 331)
    prints("at 331 m/s      : %.2f Hz\n", f_cold)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
