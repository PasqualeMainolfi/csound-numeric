<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* The operators with an audio operand: the signal takes part as its block of
   ksmps samples, an array of shape (ksmps,), as csnfromaudio would give. Each
   instrument compares against the same expression in plain Csound audio, and
   the k-rate globals carry the largest difference to the checking note. */
gkFdn0 init -1
gkFdn1 init -1
gkSubAH init -1
gkSubHA init -1
gkDivAH init -1
gkDivHA init -1
gkPowAH init -1
gkPowHA init -1
gkMulHA init -1
gkDivHH init -1
gkDivSH init -1
gkDivPack init -1

/* The feedback delay network line: a (2, 1) matrix broadcasts against the
   (ksmps,) block into (2, ksmps), one row per channel, which csnunpack turns
   back into signals. */
instr 1
    g:i = 0.9
    x:a = oscili(0.5, 440)
    ap:a = oscili(0.3, 1000)
    mat:CsnArr = csnfromarray([1, -1])
    csnreshape(mat, [2, 1])
    y:CsnArr = x + mat * (ap * g)
    ys:a[] = csnunpack(y)
    gkFdn0 = maxk(abs(ys[0] - (x + ap * g)), 1, 1)
    gkFdn1 = maxk(abs(ys[1] - (x - ap * g)), 1, 1)
endin

/* The non-commutative operators on both sides, against a (1,) array that
   broadcasts over the block. The signal stays away from zero for the
   divisions. */
instr 2
    aSig = 0.5 + oscili(0.25, 440)
    K:CsnArr = csnfromarray([2])
    aSubAH = csntoaudio(aSig - K)
    aSubHA = csntoaudio(K - aSig)
    aDivAH = csntoaudio(aSig / K)
    aDivHA = csntoaudio(K / aSig)
    aPowAH = csntoaudio(aSig ^ K)
    aPowHA = csntoaudio(K ^ aSig)
    aMulHA = csntoaudio(K * aSig)
    gkSubAH = maxk(abs(aSubAH - (aSig - 2)), 1, 1)
    gkSubHA = maxk(abs(aSubHA - (2 - aSig)), 1, 1)
    gkDivAH = maxk(abs(aDivAH - aSig / 2), 1, 1)
    gkDivHA = maxk(abs(aDivHA - 2 / aSig), 1, 1)
    gkPowAH = maxk(abs(aPowAH - aSig ^ 2), 1, 1)
    gkPowHA = maxk(abs(aPowHA - exp(aSig * log(2))), 1, 1)
    gkMulHA = maxk(abs(aMulHA - 2 * aSig), 1, 1)
endin

/* An array that csnfromaudio or csnpack fills holds no samples during the
   init pass, so what divides by it must wait for the first pass: computing
   from the placeholder zeros there would stop the note on a division by
   zero. The wait travels to what is derived from it at init (Q). */
instr 3
    aSig = 0.5 + oscili(0.25, 440)
    K:CsnArr = csnfromarray([2])
    A:CsnArr = csnfromaudio(aSig)
    Q:CsnArr = A * 1
    aDivHH = csntoaudio(K / Q)
    aDivSH = csntoaudio(2 / A)
    aIn[] init 1
    aIn[0] = aSig
    P:CsnArr = csnpack(aIn)
    aOut[] = csnunpack(K / P)
    aDivPack = aOut[0]
    gkDivHH = maxk(abs(aDivHH - 2 / aSig), 1, 1)
    gkDivSH = maxk(abs(aDivSH - 2 / aSig), 1, 1)
    gkDivPack = maxk(abs(aDivPack - 2 / aSig), 1, 1)
endin

instr 100
    iFdn0  = i(gkFdn0)
    iFdn1  = i(gkFdn1)
    iSubAH = i(gkSubAH)
    iSubHA = i(gkSubHA)
    iDivAH = i(gkDivAH)
    iDivHA = i(gkDivHA)
    iPowAH = i(gkPowAH)
    iPowHA = i(gkPowHA)
    iMulHA = i(gkMulHA)
    iDivHH = i(gkDivHH)
    iDivSH = i(gkDivSH)
    iDivPack = i(gkDivPack)

    assert(iFdn0 == 0 && iFdn1 == 0)
    assert(iSubAH == 0 && iSubHA == 0)
    /* the globals start at -1, so a note that never ran cannot pass */
    assert(iDivAH >= 0 && iDivAH < 1e-12 && iDivHA >= 0 && iDivHA < 1e-12)
    assert(iPowAH >= 0 && iPowAH < 1e-12 && iPowHA >= 0 && iPowHA < 1e-12)
    assert(iMulHA == 0)
    assert(iDivHH >= 0 && iDivHH < 1e-12 && iDivSH >= 0 && iDivSH < 1e-12)
    assert(iDivPack >= 0 && iDivPack < 1e-12)
    prints("csnum audio operators regression passed\n")
endin
</CsInstruments>

<CsScore>
i 1 0.0 0.02
i 2 0.0 0.02
i 3 0.0 0.02
i 100 0.05 0.01
e
</CsScore>
</CsoundSynthesizer>
