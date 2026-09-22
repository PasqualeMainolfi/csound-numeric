<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsphharmacn.csd
;
; Every real spherical harmonic up to an order, indexed by ACN, in the AmbiX
; convention (SN3D, no Condon-Shortley phase). Two forms:
;
;   one direction    -> a vector of (order + 1)^2 values: the encoding gains
;                       of a mono source at that direction
;   D directions     -> a ((order + 1)^2, D) matrix, one direction per column
;
; The matrix is what a mode-matching decoder is built from: for a layout of
; as many loudspeakers as channels, the decoder is its inverse (csninv), and
; for more loudspeakers the pseudo-inverse. The example builds a first-order
; decoder for a tetrahedron and checks that a source encoded exactly at one
; loudspeaker comes out of that loudspeaker alone.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; the encoding gains of a source at azimuth 30, elevation 20, order 2
    gains:CsnArr = csnsphharmacn(2, 30, 20, 1)
    ig[] = csntoarray(gains)
    prints("order 2, %d channels\n", lenarray(ig))
    prints("  n=0 : %.4f\n", ig[0])
    prints("  n=1 : %.4f %.4f %.4f\n", ig[1], ig[2], ig[3])
    prints("  n=2 : %.4f %.4f %.4f %.4f %.4f\n", ig[4], ig[5], ig[6], ig[7], ig[8])

    ; SN3D: the squares of each order sum to 1
    isq = ig[1] ^ 2 + ig[2] ^ 2 + ig[3] ^ 2
    prints("  sum of squares of order 1: %.6f\n", isq)

    ; a tetrahedron, four loudspeakers for four first-order channels
    iel = 35.26438968275466
    iaz[] = fillarray(45, 135, -135, -45)
    ielv[] = fillarray(iel, -iel, iel, -iel)
    az:CsnArr = csnfromarray(iaz)
    el:CsnArr = csnfromarray(ielv)
    Y:CsnArr = csnsphharmacn(1, az, el, 1)
    ishape[] = csnshape(Y)
    prints("direction matrix %d x %d (channels x loudspeakers)\n", ishape[0], ishape[1])

    ; mode matching: decoder = inverse of the direction matrix
    D:CsnArr = csninv(Y)

    ; a source encoded at the second loudspeaker
    icol[] = fillarray(4, 1)
    enc:CsnArr = csnsphharmacn(1, 135, -iel, 1)
    src:CsnArr = csnreshape(enc, icol)
    feeds:CsnArr = csnmatmul(D, src)
    flat:CsnArr = csnflatten(feeds)
    iF[] = csntoarray(flat)
    prints("loudspeaker feeds: %.4f %.4f %.4f %.4f\n", iF[0], iF[1], iF[2], iF[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
