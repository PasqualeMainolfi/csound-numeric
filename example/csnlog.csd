<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnlog.csd
;
; The base is the second argument. Base 2 over a frequency array gives pitch in
; octaves, which is the commonest use in an orchestra.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(8, 64, 1024))

    binary:CsnArr  = csnlog(data, 2)
    binary_out:i[] = csntoarray(binary)
    prints("log2  = %g %g %g\n", binary_out[0], binary_out[1], binary_out[2])

    dec:CsnArr     = csnlog(data, 10)
    dec_out:i[]    = csntoarray(dec)
    prints("log10 = %.4f %.4f %.4f\n", dec_out[0], dec_out[1], dec_out[2])

    ; frequencies to octaves above 55 Hz
    freq:CsnArr    = csnfromarray(array(55, 110, 440))
    rel:CsnArr     = csndiv(freq, 55)
    oct:CsnArr     = csnlog(rel, 2)
    oct_out:i[]    = csntoarray(oct)
    prints("octaves = %g %g %g\n", oct_out[0], oct_out[1], oct_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
