; Streaming UTF-8 decoder. ASCII stays on the fast path. Unicode scalars which
; have CP437 glyphs are mapped exactly; others become a single '?' glyph.
.proc plus_utf8
        lda utf_left
        bne @continue
        lda temp_char
        cmp #$c2
        jcc @bad
        cmp #$e0
        bcc @two
        cmp #$f0
        bcc @three
        cmp #$f5
        jcs @bad
        and #7
        sta utf_lo
        lda #3
        sta utf_left
        lda temp_char
        cmp #$f0
        bne :+
        lda #$90
        bne @min
:       cmp #$f4
        bne @standard
        lda #$8f
        sta utf_max
        lda #$80
        bne @low
@three: and #15
        sta utf_lo
        lda #2
        sta utf_left
        lda temp_char
        cmp #$e0
        bne :+
        lda #$a0
        bne @min
:       cmp #$ed
        bne @standard
        lda #$9f
        sta utf_max
        lda #$80
        bne @low
@two:  and #31
        sta utf_lo
        lda #1
        sta utf_left
@standard:lda #$80
@min:   sta utf_min
        lda #$bf
        sta utf_max
        lda utf_min
@low:   sta utf_min
        lda #0
        sta utf_hi
        sta utf_wide
        rts
@continue:
        lda temp_char
        cmp utf_min
        bcc @bad_restart
        cmp utf_max
        beq :+
        bcs @bad_restart
:       and #63
        sta plus_digit
        ldx #6
@shift: asl utf_lo
        rol utf_hi
        rol utf_wide
        dex
        bne @shift
        lda utf_lo
        ora plus_digit
        sta utf_lo
        lda #$80
        sta utf_min
        lda #$bf
        sta utf_max
        dec utf_left
        bne @return
        lda utf_wide
        jne u_emit
        ldx #0
@find:  lda unicode_hi,x
        cmp utf_hi
        bne @next
        lda unicode_lo,x
        cmp utf_lo
        beq @found
@next:  inx
        bne @find
        jmp u_emit
@bad:   lda #0
        sta utf_left
        sta utf_wide
        lda #'?'
        sta temp_char
        jmp plus_put
@found: stx temp_char
        jmp plus_put
@return:rts
@bad_restart:
        lda temp_char
        pha
        jsr @bad
        pla
        sta temp_char
        cmp #$80
        bcc @restart
        cmp #$c2
        bcc @return
@restart:jmp plus_process
.endproc
        .include "unicode_tables.inc"
