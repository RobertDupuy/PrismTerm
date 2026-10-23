; Production row operations: VBXE blits move cells; CPU moves optional styles.
; The original portable CPU primitives remain in scroll_rgn.inc for reference.
row_addr_lo: .repeat 24,r
 .byte <(vbxe_screen_top+r*160)
 .endrepeat
row_addr_hi: .repeat 24,r
 .byte >(vbxe_screen_top+r*160)
 .endrepeat
.proc reset_margins
 lda #0
 sta scroll_top
 lda #23
 sta scroll_bot
 rts
.endproc
.proc plus_region_prepare
 sta sr_top
 stx sr_bot
 sty sr_n
 txa
 sec
 sbc sr_top
 clc
 adc #1
 cmp sr_n
 bcs :+
 sta sr_n
: sec
 sbc sr_n
 sta sr_rows
 rts
.endproc
; A = CPU high byte of visible cell address -> VRAM middle byte.
.proc plus_vram_hi
 sec
 sbc #$a0
 clc
 adc #$f0
 rts
.endproc
.proc plus_region_blit_setup
 jsr plus_blit_reset
 lda #7
 sta blit_cfg+2
 sta blit_cfg+8
 lda #159
 sta blit_cfg+12
 lda #160
 sta blit_cfg+3
 sta blit_cfg+9
 lda sr_rows
 sec
 sbc #1
 sta blit_cfg+14
 rts
.endproc
.proc scroll_rgn_up
 cpy #0
 beq @out
 jsr plus_region_prepare
 lda sr_rows
 beq @blank
 jsr plus_region_blit_setup
 ldx sr_top
 lda row_addr_lo,x
 sta blit_cfg+6
 lda row_addr_hi,x
 jsr plus_vram_hi
 sta blit_cfg+7
 txa
 clc
 adc sr_n
 tax
 lda row_addr_lo,x
 sta blit_cfg
 lda row_addr_hi,x
 jsr plus_vram_hi
 sta blit_cfg+1
 jsr plus_blit
 jsr plus_styles_copy_up
@blank: lda sr_bot
 sec
 sbc sr_n
 clc
 adc #1
 ldx sr_n
 jmp blank_rows
@out: rts
.endproc
.proc scroll_rgn_down
 cpy #0
 beq @out
 jsr plus_region_prepare
 lda sr_rows
 beq @blank
 jsr plus_region_blit_setup
 lda #96
 sta blit_cfg+3
 sta blit_cfg+9
 lda #255
 sta blit_cfg+4
 sta blit_cfg+10
 ldx sr_bot
 lda row_addr_lo,x
 sta blit_cfg+6
 lda row_addr_hi,x
 jsr plus_vram_hi
 sta blit_cfg+7
 txa
 sec
 sbc sr_n
 tax
 lda row_addr_lo,x
 sta blit_cfg
 lda row_addr_hi,x
 jsr plus_vram_hi
 sta blit_cfg+1
 jsr plus_blit
 jsr plus_styles_copy_down
@blank: lda sr_top
 ldx sr_n
 jmp blank_rows
@out: rts
.endproc
.proc blank_rows
 cpx #0
 beq @out
 sta vrow
 stx vline
 jsr plus_blit_reset
 ldx vrow
 lda row_addr_lo,x
 sta blit_cfg+6
 lda row_addr_hi,x
 jsr plus_vram_hi
 sta blit_cfg+7
 lda #7
 sta blit_cfg+8
 lda #160
 sta blit_cfg+9
 lda #2
 sta blit_cfg+11
 lda #79
 sta blit_cfg+12
 lda vline
 sec
 sbc #1
 sta blit_cfg+14
 lda #0
 sta blit_cfg+15
 jsr plus_blit
 inc blit_cfg+6
 lda text_color
 sta blit_cfg+16
 jsr plus_blit
 lda vrow
 ldx vline
 jmp plus_styles_blank
@out: rts
.endproc
; Metadata-only counterpart of the fast full-screen native scroll.
.proc plus_styles_up
 jsr plus_region_prepare
 jsr plus_styles_copy_up
 lda sr_bot
 sec
 sbc sr_n
 clc
 adc #1
 ldx sr_n
 jmp plus_styles_blank
.endproc
.proc plus_styles_copy_up
 jsr u_scroll_up
 lda bitmap_on
 beq @out
 lda sr_top
 sta vrow
 lda sr_rows
 sta vline
 beq @out
@row: ldx vrow
 jsr plus_dirty_row
 lda style_lo,x
 sta vptr
 lda style_hi,x
 sta vptr+1
 txa
 clc
 adc sr_n
 tax
 lda blink_rows,x
 ldy vrow
 sta blink_rows,y
 lda style_lo,x
 sta sptr
 lda style_hi,x
 sta sptr+1
 ldy #79
@copy: lda (sptr),y
 sta (vptr),y
 dey
 bpl @copy
 inc vrow
 dec vline
 bne @row
@out: rts
.endproc
.proc plus_styles_copy_down
 jsr u_scroll_down
 lda bitmap_on
 beq @out
 lda sr_bot
 sta vrow
 lda sr_rows
 sta vline
 beq @out
@row: ldx vrow
 jsr plus_dirty_row
 lda style_lo,x
 sta vptr
 lda style_hi,x
 sta vptr+1
 txa
 sec
 sbc sr_n
 tax
 lda blink_rows,x
 ldy vrow
 sta blink_rows,y
 lda style_lo,x
 sta sptr
 lda style_hi,x
 sta sptr+1
 ldy #79
@copy: lda (sptr),y
 sta (vptr),y
 dey
 bpl @copy
 dec vrow
 dec vline
 bne @row
@out: rts
.endproc
.proc plus_styles_blank
 cpx #0
 beq @out
 sta vrow
 stx vline
 lda bitmap_on
 bne @start
 lda draw_bg
 and #8
 beq @out
 lda vrow
 pha
 lda vline
 pha
 jsr plus_enable_bitmap
 pla
 sta vline
 pla
 sta vrow
@start: lda draw_bg
 and #8
 asl
 sta vstyle
@row: ldx vrow
 jsr plus_dirty_row
 lda #0
 sta blink_rows,x
 lda style_lo,x
 sta vptr
 lda style_hi,x
 sta vptr+1
 ldy #79
 lda vstyle
@clear: sta (vptr),y
 dey
 bpl @clear
 inc vrow
 dec vline
 bne @row
@out: rts
.endproc
; Shared row pointers, with a separate base for styles.
.proc row_setup_ptrs
 ldx row
 lda row_addr_lo,x
 sta scr_dst
 lda row_addr_hi,x
 sta scr_dst+1
 lda style_lo,x
 sta sptr
 lda style_hi,x
 sta sptr+1
 jmp plus_dirty_row
.endproc
.proc row_insert_char
 lda #1
 sta plus_count
 jmp plus_insert_chars
.endproc
.proc row_delete_char
 lda #1
 sta plus_count
 jmp plus_delete_chars
.endproc
.proc plus_insert_chars
 jsr row_setup_ptrs
 lda term_last_col
 sec
 sbc plus_count
 bcc @blank
 cmp column
 bcc @blank
 sta vcol
@move: jsr u_insert_cell
 lda vcol
 asl
 tay
 lda (scr_dst),y
 sta vchar
 iny
 lda (scr_dst),y
 sta vattr
 lda vcol
 clc
 adc plus_count
 asl
 tay
 lda vchar
 sta (scr_dst),y
 iny
 lda vattr
 sta (scr_dst),y
 lda bitmap_on
 beq :+
 ldy vcol
 lda (sptr),y
 sta vstyle
 tya
 clc
 adc plus_count
 tay
 lda vstyle
 sta (sptr),y
: lda vcol
 cmp column
 beq @blank
 dec vcol
 jmp @move
@blank: jmp plus_erase_chars
.endproc
.proc plus_delete_chars
 jsr row_setup_ptrs
 lda column
 sta vcol
@move: lda vcol
 clc
 adc plus_count
 cmp term_last_col
 bcc @copy
 bne @blank
@copy: sta vbyte
 jsr u_delete_cell
 lda vbyte
 asl
 tay
 lda (scr_dst),y
 sta vchar
 iny
 lda (scr_dst),y
 sta vattr
 lda vcol
 asl
 tay
 lda vchar
 sta (scr_dst),y
 iny
 lda vattr
 sta (scr_dst),y
 lda bitmap_on
 beq :+
 ldy vbyte
 lda (sptr),y
 ldy vcol
 sta (sptr),y
: inc vcol
 jmp @move
@blank: lda column
 pha
 lda vcol
 sta column
 jsr recalc_cursor
 jsr plus_erase_chars
 pla
 sta column
 jmp recalc_cursor
.endproc
.proc plus_erase_chars
 lda bitmap_on
 bne :+
 lda draw_bg
 and #8
 beq :+
 jsr plus_enable_bitmap
: jsr row_setup_ptrs
 lda draw_bg
 and #8
 asl
 sta vstyle
 lda column
 sta vcol
 lda plus_count
 sta vline
@clear: lda vcol
 asl
 tay
 lda #0
 sta (scr_dst),y
 iny
 lda text_color
 sta (scr_dst),y
 lda bitmap_on
 beq :+
 ldy vcol
 lda vstyle
 sta (sptr),y
: inc vcol
 dec vline
 bne @clear
 rts
.endproc
