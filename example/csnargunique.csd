<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnargunique.csd
;
; The positions line up with csnunique's values, which is what lets a parallel
; array be de-duplicated the same way.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(3, 1, 4, 1, 5))

    values:CsnArr  = csnunique(data)
    where:CsnArr   = csnargunique(data)
    values_out:i[] = csntoarray(values)
    where_out:i[]  = csntoarray(csnflatten(where))
    count:i        = csnsize(values)

    prints("distinct = %d\n", count)
    n:i = 0
    while n < count do
        prints("value %g first seen at index %g\n", values_out[n], where_out[n])
        n += 1
    od

    ; the same positions pick out of a parallel array
    tags:CsnArr    = csnfromarray(array(10, 20, 30, 40, 50))
    first_tag:i    = csntake(tags, where_out[0])
    prints("tag of the first distinct value = %g\n", first_tag)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
