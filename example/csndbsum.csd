<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csndbsum.csd
;
; Decibels do not add. Two equal sources make one 3.01 dB louder, not twice the
; number: the powers add, and the sum goes back to dB. csndbsum does that on a
; pair of levels, over a whole array, or along one axis of a matrix.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; two equal sources: +3.0103 dB, the classic doubling
    pair:i          = csndbsum(80, 80)
    prints("80 and 80       : %.4f dB\n", pair)

    ; 6 dB apart: the quieter one barely counts
    uneven:i        = csndbsum(85, 79)
    prints("85 and 79       : %.4f dB\n", uneven)

    ; 20 dB apart: it counts for almost nothing at all
    far:i           = csndbsum(85, 65)
    prints("85 and 65       : %.4f dB\n", far)

    ; a whole array of sources at once: four equal ones make +6.0206 dB
    four:CsnArr     = csnfromarray(array(80, 80, 80, 80))
    total:i         = csndbsum(four)
    prints("four times 80   : %.4f dB\n", total)

    ; an octave-band spectrum, summed to one broadband level
    bands:CsnArr    = csnfromarray(array(72, 78, 81, 76, 69, 61))
    broadband:i     = csndbsum(bands)
    prints("broadband       : %.4f dB\n", broadband)

    ; one row per source, one column per band: axis 0 sums the sources band by
    ; band, axis 1 sums the bands source by source
    shape:i[]       = fillarray(2, 3)
    matrix:CsnArr   = csnreshape(csnfromarray(array(70, 76, 80, 70, 70, 74)), shape)
    per_band:CsnArr = csndbsum(matrix, 0)
    per_source:CsnArr = csndbsum(matrix, 1)
    csnprint per_band
    csnprint per_source
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
