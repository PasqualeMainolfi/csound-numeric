<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

; chr takes an ASCII code (1 to 127, whole), ord a one-byte string, and
; fromascii a real 1-D array of ASCII codes; a search range is checked as on
; an array; UTF-8 is checked as Python's strict decoder does, and code points
; stop at U+10FFFF, without the surrogates. Each refusal leaves nothing
; locked.

instr 1
    SOut = csnstrchr(0)
endin

instr 2
    SOut = csnstrchr(65.5)
endin

instr 3
    iCode = csnstrord("ab")
endin

instr 4
    Codes:CsnArr = csnfromarray(array(72, 200))
    SOut = csnstrfromascii(Codes)
endin

instr 5
    Real:CsnArr = csnfromarray(array(72, 105))
    Codes:CsnArr = csntocomplex(Real)
    SOut = csnstrfromascii(Codes)
endin

instr 6
    iWhere = csnstrfind("abc", "b", 2, 1)
endin

instr 7
    Bytes:CsnArr = csnfromarray(array(65, 192, 128))
    SOut = csnstrdecode(Bytes)
endin

instr 8
    Points:CsnArr = csnfromarray(array(55296))
    SOut = csnstrfromcodepoints(Points)
endin

; a lone lead byte, 0xC3, is not UTF-8
instr 9
    Points:CsnArr = csnstrtocodepoints("√")
endin

; an overlong form, a surrogate, a code point past U+10FFFF and a cut
; sequence are not UTF-8, as in Python
instr 10
    Bytes:CsnArr = csnfromarray(array(224, 128, 128))
    SOut = csnstrdecode(Bytes)
endin

instr 11
    Bytes:CsnArr = csnfromarray(array(237, 160, 128))
    SOut = csnstrdecode(Bytes)
endin

instr 12
    Bytes:CsnArr = csnfromarray(array(244, 144, 128, 128))
    SOut = csnstrdecode(Bytes)
endin

instr 13
    Bytes:CsnArr = csnfromarray(array(97, 226, 130))
    SOut = csnstrdecode(Bytes)
endin

instr 14
    Bytes:CsnArr = csnstrencode("a√")
endin

; U+FFFF written in four bytes is an overlong form too
instr 15
    Bytes:CsnArr = csnfromarray(array(240, 143, 191, 191))
    SOut = csnstrdecode(Bytes)
endin

instr 16
    Codes:CsnArr = csnfromarray(array(79, 75))
    SOut = csnstrfromascii(Codes)
    iCmp = strcmp(SOut, "OK")
    assert(iCmp == 0)
    prints("csnum S queries alive after every refusal\n")
endin
</CsInstruments>

<CsScore>
i1 0    0.01
i2 0.02 0.01
i3 0.04 0.01
i4 0.06 0.01
i5 0.08 0.01
i6 0.10 0.01
i7 0.12 0.01
i8 0.14 0.01
i9 0.16 0.01
i10 0.18 0.01
i11 0.20 0.01
i12 0.22 0.01
i13 0.24 0.01
i14 0.26 0.01
i15 0.28 0.01
i16 0.30 0.01
</CsScore>
</CsoundSynthesizer>
