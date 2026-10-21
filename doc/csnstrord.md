# csnstrord

## Abstract

The byte of a one-byte string, as Python's `ord` on bytes.

## Description

`csnstrord` returns the value of the single byte of `source`, from 0 to 255:
`csnstrord("A")` gives 65. The string must hold exactly one byte: an empty
string or a longer one is refused, and so is a character that UTF-8 writes in
more than one byte, such as `"é"`; [csnstrtoascii](csnstrtoascii.md) gives all
of its bytes. Csound's `strchar` reads a byte at a position of a longer
string.

The `i` output takes the init-time form, a `k` output the k form. An `S` has
no rate, so the k form takes a trigger, and computes at init too, when the
string holds one byte there: an `S` that a k opcode writes may still be empty
at init, and the first pass then judges it.

## Syntax

```csound
out:i = csnstrord(source:S)
out:k = csnstrord(source:S [, trig:k])
```

## Arguments

* `source:S`: a string of one byte.
* `trig:k`: k-rate trigger, optional (default `1`). A zero trigger keeps the previous value.

## Output

* `out:i`, `out:k`: the byte, from 0 to 255.

## Execution Time

* Init
* Performance (k-rate)

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnstrord.csd
;
; The byte of a pitch letter: "A" is 65.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    letter:S = "A"
    code:i   = csnstrord(letter)
    prints("%s is %d\n", letter, code)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
A is 65
```

## See also

* [csnstrchr](csnstrchr.md)
* [csnstrtoascii](csnstrtoascii.md)
* [string arrays](string-arrays.md)

## Credits

Pasquale Mainolfi, 2026
