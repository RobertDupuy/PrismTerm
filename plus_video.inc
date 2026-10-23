; Optional bitmap renderer. Native text stays the default fast path.
; VRAM font masks 10000..1BFFF; framebuffer 20000..2EFFF.
vptr=$c0
fptr=$c2
sptr=$c4
vtemp=$c6
.segment "BSS"
draw_fg: .res 1
draw_bg: .res 1
bitmap_on: .res 1
fonts_ready: .res 1
blink_phase: .res 1
blink_clock: .res 1
blink_rows: .res 24
dirty_any: .res 1
dirty_rows: .res 24
dirty_first: .res 24
dirty_last: .res 24
cell_styles: .res 1920
vrow: .res 1
vcol: .res 1
vchar: .res 1
vattr: .res 1
vstyle: .res 1
vbank: .res 1
vglyph: .res 1
vformat: .res 1
vline: .res 1
vbits: .res 1
vbyte: .res 1
glyph_raw: .res 8
.segment "CODE"
.segment "RESIDENT"
blit_cfg:
 .faraddr 0
 .word 0
 .byte 1
 .faraddr 0
 .word 0
 .byte 1
 .word 0
 .byte 0,$ff,0,0,0,0,0
.segment "CODE"
.proc plus_blit
 lda memac_bank_sel
 pha
 lda #$80
 sta memac_bank_sel
 ldx #20
@copy: lda blit_cfg,x
 sta $ac00,x
 dex
 bpl @copy
 lda #0
 sta blt_adr
 sta blt_adr+2
 lda #12
 sta blt_adr+1
 lda #1
 sta blt_start
@wait: lda blt_busy
 bne @wait
 pla
 sta memac_bank_sel
 rts
.endproc
.proc plus_blit_reset
 lda #0
 ldx #20
@loop: sta blit_cfg,x
 dex
 bpl @loop
 lda #1
 sta blit_cfg+5
 sta blit_cfg+11
 lda #255
 sta blit_cfg+15
 rts
.endproc
.proc plus_video_init
 lda #0
 sta u_active
 sta u_disk_ok
 sta u_cached_sector
 sta u_cached_sector+1
 sta u_last_width
 sta bitmap_on
 sta fonts_ready
 sta blink_phase
 sta blink_clock
 sta dirty_any
 ldx #23
@rows: sta dirty_rows,x
 sta blink_rows,x
 sta dirty_last,x
 sta dirty_first,x
 dex
 bpl @rows
 lda #<cell_styles
 sta vptr
 lda #>cell_styles
 sta vptr+1
 ldx #15
 ldy #0
 lda #0
@clear: sta (vptr),y
 iny
 cpy #128
 bne @clear
 tya
 clc
 adc vptr
 sta vptr
 bcc :+
 inc vptr+1
: ldy #0
 lda #0
 dex
 bne @clear
 jsr plus_native_display
 rts
.endproc
.proc plus_style_ptr
 ldx row
 lda style_lo,x
 clc
 adc column
 sta sptr
 lda style_hi,x
 adc #0
 sta sptr+1
 rts
.endproc
.proc plus_cell_style
 lda bitmap_on
 bne @active
 lda plus_style
 bne @enable
 lda draw_bg
 and #8
 bne @enable
 lda term_last_col
 cmp #39
 bne @ret
@enable: jsr plus_enable_bitmap
@active: jsr plus_style_ptr
 lda draw_bg
 and #8
 asl
 ora plus_style
 ldy #0
 sta (sptr),y
 ldx row
 and #2
 beq :+
 lda #1
 sta blink_rows,x
: lda column
 jmp plus_dirty_cell
@ret: rts
.endproc
.proc plus_dirty_cursor
 lda bitmap_on
 beq @out
 txa
 pha
 ldx row
 lda column
 jsr plus_dirty_cell
 pla
 tax
@out: rts
.endproc
; A=column, X=row. Coalesce a receive burst to one contiguous dirty span.
.proc plus_dirty_cell
 pha
 lda #1
 sta dirty_any
 pla
 ldy dirty_rows,x
 bne @merge
 sta dirty_first,x
 sta dirty_last,x
 inc dirty_rows,x
 rts
@merge: cmp dirty_first,x
 bcs :+
 sta dirty_first,x
: cmp dirty_last,x
 bcc :+
 sta dirty_last,x
: rts
.endproc
.proc plus_dirty_row
 lda #0
 sta dirty_first,x
 lda term_last_col
 sta dirty_last,x
 lda #1
 sta dirty_any
 sta dirty_rows,x
 rts
.endproc
.proc plus_dirty_all
 lda bitmap_on
 beq @ret
 ldx #23
@loop: jsr plus_dirty_row
 dex
 bpl @loop
@ret: rts
.endproc
.proc plus_native_display
 lda memac_bank_sel
 pha
 lda #$80
 sta memac_bank_sel
 lda #$61
 sta $a805
 lda #$81
 sta $a806
 lda #0
 sta $a808
 sta $a80c
 lda #$f1
 sta $a809
 lda #7
 sta $a80a
 lda #160
 sta $a80b
 pla
 sta memac_bank_sel
 rts
.endproc
.proc plus_enable_bitmap
 lda #1
 ldx term_last_col
 cpx #39
 bne :+
 lda #2
: sta bitmap_on
 lda fonts_ready
 bne :+
 jsr plus_expand_fonts
: jsr plus_dirty_all
 lda memac_bank_sel
 pha
 lda #$80
 sta memac_bank_sel
 lda #$62
 sta $a805
 lda #$90
 ldx bitmap_on
 cpx #2
 bne :+
 lda #$80
: sta $a806
 lda #0
 sta $a808
 sta $a809
 lda #2
 sta $a80a
 lda #64
 sta $a80b
 lda #1
 sta $a80c
 pla
 sta memac_bank_sel
 rts
.endproc
.proc plus_expand_fonts
 lda memac_bank_sel
 pha
 lda #0
 sta vformat
@format: ldx vformat
 lda font_banks,x
 sta vbank
 lda #0
 sta vglyph
 sta vptr
 sta fptr
 lda #$a0
 sta vptr+1
 sta fptr+1
@glyph: lda #$80
 sta memac_bank_sel
 ldy #7
@raw: lda (fptr),y
 sta glyph_raw,y
 dey
 bpl @raw
 lda fptr
 clc
 adc #8
 sta fptr
 bcc :+
 inc fptr+1
: lda vformat
 and #1
 beq :+
 lda #255
 sta glyph_raw+7
: lda vbank
 sta memac_bank_sel
 lda #0
 sta vline
@line: ldx vline
 lda glyph_raw,x
 sta vbits
 lda #8
 sta vbyte
 ldy #0
 lda vformat
 cmp #2
 bcc @hires
@std: asl vbits
 lda #0
 sbc #0
 eor #255
 sta (vptr),y
 iny
 dec vbyte
 bne @std
 beq @advance
@hires: asl vbits
 lda #0
 sbc #0
 eor #255
 and #$f0
 sta vtemp
 asl vbits
 lda #0
 sbc #0
 eor #255
 and #15
 ora vtemp
 sta (vptr),y
 iny
 dec vbyte
 dec vbyte
 bne @hires
@advance: tya
 clc
 adc vptr
 sta vptr
 bcc :+
 inc vptr+1
 lda vptr+1
 cmp #$b0
 bcc :+
 lda #$a0
 sta vptr+1
 inc vbank
 lda vbank
 sta memac_bank_sel
: inc vline
 lda vline
 cmp #8
 bcc @line
 inc vglyph
 jne @glyph
 inc vformat
 lda vformat
 cmp #4
 jcc @format
 lda #1
 sta fonts_ready
 pla
 sta memac_bank_sel
 rts
.endproc
font_banks: .byte $90,$92,$94,$98
.proc plus_render_dirty
 lda bitmap_on
 beq @out
 lda dirty_any
 bne :+
@out: rts
: lda #0
 sta dirty_any
 jsr plus_render_setup
 lda #0
 sta vrow
@row: ldx vrow
 lda dirty_rows,x
 jeq @nextrow
 lda #0
 sta dirty_rows,x
 lda row_addr_lo,x
 sta fptr
 lda row_addr_hi,x
 sta fptr+1
 lda style_lo,x
 sta sptr
 lda style_hi,x
 sta sptr+1
 lda dirty_first,x
 sta vcol
@col: lda vcol
 asl
 tay
 lda (fptr),y
 sta vchar
 iny
 lda (fptr),y
 sta vattr
 ldy vcol
 lda (sptr),y
 sta vstyle
 jsr plus_render_glyph
 inc vcol
 ldx vrow
 lda dirty_last,x
 cmp vcol
 jcs @col
@nextrow: inc vrow
 lda vrow
 cmp #24
 jcc @row
 rts
.endproc
.proc plus_render_glyph
 ldx vchar
 lda glyph_offset_lo,x
 sta blit_cfg
 lda glyph_offset_hi,x
 sta blit_cfg+1
 lda bitmap_on
 cmp #2
 bne @hr
 asl blit_cfg
 rol blit_cfg+1
 lda #$40
 clc
 adc blit_cfg+1
 sta blit_cfg+1
 lda vstyle
 and #1
 beq @dest
 lda #$40
 bne @underline
@hr: lda vstyle
 and #1
 beq @dest
 lda #$20
@underline: clc
 adc blit_cfg+1
 sta blit_cfg+1
@dest:
 ldx vrow
 lda pixel_row_lo,x
 sta blit_cfg+6
 lda pixel_row_hi,x
 sta blit_cfg+7
 lda #2
 sta blit_cfg+8
 lda #0
 sta vtemp
 lda vcol
 asl
 rol vtemp
 asl
 rol vtemp
 ldx bitmap_on
 cpx #2
 bne :+
 asl
 rol vtemp
: clc
 adc blit_cfg+6
 sta blit_cfg+6
 lda blit_cfg+7
 adc vtemp
 sta blit_cfg+7
 lda #64
 sta blit_cfg+9
 lda #1
 sta blit_cfg+10
 lda vattr
 lsr
 lsr
 lsr
 lsr
 and #7
 sta vtemp
 lda vstyle
 and #16
 lsr
 ora vtemp
 sta vtemp
 ldx bitmap_on
 cpx #2
 beq :+
 asl
 asl
 asl
 asl
 ora vtemp
: sta blit_cfg+16
 lda vattr
 and #15
 sta vtemp
 ldx bitmap_on
 cpx #2
 beq :+
 asl
 asl
 asl
 asl
 ora vtemp
: eor blit_cfg+16
 sta blit_cfg+15
 lda vstyle
 and #2
 beq @draw
 lda blink_phase
 beq @draw
 lda #0
 sta blit_cfg+15
@draw: jsr u_render_source
 jmp plus_glyph_blit
.endproc
style_lo: .repeat 24,r
 .byte <(cell_styles+r*80)
 .endrepeat
style_hi: .repeat 24,r
 .byte >(cell_styles+r*80)
 .endrepeat
pixel_row_lo: .repeat 24,r
 .byte <(r*2560)
 .endrepeat
pixel_row_hi: .repeat 24,r
 .byte >(r*2560)
 .endrepeat

.proc plus_styles_snapshot
 sta session_bank
 stx io_page
 lda memac_bank_sel
 pha
 lda session_bank
 sta memac_bank_sel
 lda #<cell_styles
 sta vptr
 lda #>cell_styles
 sta vptr+1
 lda #0
 sta sptr
 lda #$a0
 sta sptr+1
 ldx #15
@block: ldy #0
@byte: lda io_page
 bne @restore
 lda (vptr),y
 sta (sptr),y
 jmp @next
@restore: lda (sptr),y
 sta (vptr),y
@next: iny
 cpy #128
 bne @byte
 lda vptr
 clc
 adc #128
 sta vptr
 bcc :+
 inc vptr+1
: lda sptr
 clc
 adc #128
 sta sptr
 bcc :+
 inc sptr+1
: dex
 bne @block
 pla
 sta memac_bank_sel
 lda io_page
 beq @out
 ldx #23
@flags: jsr plus_blink_row
 dex
 bpl @flags
@out: jmp plus_dirty_all
.endproc
; Rebuild cached masks after switching the underlying native font.
.proc plus_font_changed
 lda u_active
 beq :+
 jmp plus_dirty_all
: jsr plus_expand_fonts
 lda bitmap_on
 bne @bitmap
 lda term_last_col
 cmp #39
 bne @out
@bitmap: jmp plus_enable_bitmap
@out: rts
.endproc

.proc plus_menu_styles
 lda menu_row
 sta vrow
 lda menu_h
 sta vline
@row: ldx vrow
 lda style_lo,x
 sta sptr
 lda style_hi,x
 sta sptr+1
 ldy menu_col
 ldx menu_w
 lda #0
@cell: sta (sptr),y
 iny
 dex
 bne @cell
 inc vrow
 dec vline
 bne @row
 jmp plus_dirty_all
.endproc

; Configure fixed glyph dimensions and strides once per render pass.
.proc plus_render_setup
 jsr plus_blit_reset
 lda #1
 sta blit_cfg+2
 lda #2
 sta blit_cfg+8
 lda #4
 ldx bitmap_on
 cpx #2
 bne :+
 lda #8
: sta blit_cfg+3
 sec
 sbc #1
 sta blit_cfg+12
 lda #7
 sta blit_cfg+14
 lda #64
 sta blit_cfg+9
 lda #1
 sta blit_cfg+10
 lda memac_bank_sel
 pha
 lda #$80
 sta memac_bank_sel
 ldx #20
@copy: lda blit_cfg,x
 sta $ac00,x
 dex
 bpl @copy
 lda #0
 sta blt_adr
 sta blt_adr+2
 lda #12
 sta blt_adr+1
 pla
 sta memac_bank_sel
 rts
.endproc
.proc plus_glyph_blit
 lda memac_bank_sel
 pha
 lda #$80
 sta memac_bank_sel
 ; Only six BCB bytes vary between neighboring glyphs.
 lda blit_cfg
 sta $ac00
 lda blit_cfg+1
 sta $ac01
 lda blit_cfg+2
 sta $ac02
 lda blit_cfg+6
 sta $ac06
 lda blit_cfg+7
 sta $ac07
 lda blit_cfg+15
 sta $ac0f
 lda blit_cfg+16
 sta $ac10
 lda #1
 sta blt_start
@wait: lda blt_busy
 bne @wait
 pla
 sta memac_bank_sel
 rts
.endproc
glyph_offset_lo: .repeat 256,g
 .byte <(g*32)
 .endrepeat
glyph_offset_hi: .repeat 256,g
 .byte >(g*32)
 .endrepeat

.proc plus_center_menu
 lda term_last_col
 sec
 sbc menu_w
 clc
 adc #1
 lsr
 sta menu_col
 rts
.endproc
.proc plus_recenter_menu
 jsr restore_under_rect
 lda #$fe
 ldx #1
 jsr plus_styles_snapshot
 jsr plus_center_menu
 jsr save_under_rect
 jsr plus_menu_styles
 jmp menu_draw
.endproc

; Blink only spans that actually contain blinking cells. Clear stale row flags.
.proc plus_blink_row
 lda style_lo,x
 sta sptr
 lda style_hi,x
 sta sptr+1
 lda #0
 sta blink_rows,x
 ldy #0
@cell: lda (sptr),y
 and #2
 beq @next
 lda #1
 sta blink_rows,x
 sty vcol
 tya
 jsr plus_dirty_cell
 ldy vcol
@next: iny
 cpy term_last_col
 bcc @cell
 beq @cell
 rts
.endproc
