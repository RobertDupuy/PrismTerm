u_i:.byte 0
u_j:.byte 0
u_bits:.byte 0
u_byte:.byte 0
u_bank:.byte 0
u_hash:.byte 0
u_slot:.byte 0
u_histrow:.byte 0
u_work:.res 8,0
u_ptr_bank: rts
.proc u_init_bank
 jsr b_blit_reset
 lda #$e0
 sta blit_cfg+7
 lda #0
 sta blit_cfg+8
 sta blit_cfg+15
 lda #255
 sta blit_cfg+12
 lda #31
 sta blit_cfg+14
 lda #1
 sta blit_cfg+10
 jmp b_blit
.endproc
.proc u_cache_ptr
 lda utf_lo
 eor utf_hi
 eor utf_wide
 sta u_hash
 lda #0
 sta uptr+1
 lda u_hash
 asl
 rol uptr+1
 asl
 rol uptr+1
 asl
 rol uptr+1
 asl
 rol uptr+1
 asl
 rol uptr+1
 sta uptr
 lda uptr+1
 clc
 adc #$e0
 pha
 lsr
 lsr
 lsr
 lsr
 ora #128
 sta memac_bank_sel
 pla
 and #15
 ora #$a0
 sta uptr+1
 rts
.endproc
.proc u_lookup_bank
 lda memac_bank_sel
 pha
 jsr u_cache_ptr
 ldy #29
 lda (uptr),y
 cmp utf_lo
 bne @miss
 iny
 lda (uptr),y
 cmp utf_hi
 bne @miss
 iny
 lda (uptr),y
 cmp utf_wide
 bne @miss
 ldy #0
 lda (uptr),y
 jmi @hit
@miss:
 lda u_disk_ok
 cmp #2
 jeq @fallback
 cmp #1
 beq @directory
 lda #1
 sta u_sector_no
 lda #0
 sta u_sector_no+1
 jsr b_udisk
 lda u_disk_error
 jmi @diskfail
 lda u_sector
 cmp #'U'
 jne @diskfail
 lda u_sector+1
 cmp #'F'
 jne @diskfail
 lda u_sector+2
 cmp #'N'
 jne @diskfail
 lda u_sector+3
 cmp #'1'
 jne @diskfail
 lda #1
 sta u_disk_ok
@directory:
 lda utf_hi
 asl
 lda utf_wide
 rol
 cmp u_dir_page
 beq @page
 sta u_dir_page
 clc
 adc #4
 sta u_sector_no
 lda #0
 sta u_sector_no+1
 jsr b_udisk
 lda u_disk_error
 jmi @diskfail
 ldx #0
@copydir:lda u_sector,x
 sta u_directory,x
 inx
 bne @copydir
@page:
 lda utf_hi
 and #127
 asl
 tax
 lda u_directory,x
 sta u_sector_no
 lda u_directory+1,x
 sta u_sector_no+1
 ora u_sector_no
 jeq @fallback
 lda utf_lo
 lsr
 lsr
 lsr
 clc
 adc u_sector_no
 sta u_sector_no
 bcc :+
 inc u_sector_no+1
: jsr b_udisk
 lda u_disk_error
 jmi @diskfail
 lda utf_lo
 and #7
 asl
 asl
 asl
 asl
 asl
 tax
 lda u_sector,x
 jpl @fallback
 stx u_j
 jsr u_cache_ptr
 ldx u_j
 ldy #0
@record:lda u_sector,x
 sta (uptr),y
 inx
 iny
 cpy #29
 bcc @record
 lda utf_lo
 sta (uptr),y
 iny
 lda utf_hi
 sta (uptr),y
 iny
 lda utf_wide
 sta (uptr),y
@hit:
 ldy #0
 lda (uptr),y
 sta u_flags
 ldx #0
 iny
@glyph:lda (uptr),y
 sta u_glyph,x
 inx
 iny
 cpx #16
 bcc @glyph
 pla
 sta memac_bank_sel
 rts
@diskfail:lda #2
 sta u_disk_ok
@fallback:
 ; Explicit replacement glyph; missing font coverage is never transliterated.
 lda #$81
 sta u_flags
 ldx #15
@box:lda u_replacement,x
 sta u_glyph,x
 dex
 bpl @box
 pla
 sta memac_bank_sel
 rts
.endproc
u_replacement:.byte $7e,0,$42,0,$5a,0,$52,0,$4a,0,$42,0,$7e,0,0,0
; Load the bitmap represented by vrow/vcol into u_temp. Legacy cells still
; use the selected native font, preserving font switching and ATASCII.
.proc u_raw_cell
 lda memac_bank_sel
 pha
 lda vstyle
 bmi @custom
 lda #$80
 sta memac_bank_sel
 lda #0
 sta uptr+1
 lda vchar
 asl
 rol uptr+1
 asl
 rol uptr+1
 asl
 rol uptr+1
 sta uptr
 lda uptr+1
 ora #$a0
 sta uptr+1
 jmp @copy
@custom:lda vrow
 ldx vcol
 jsr u_pointer
@copy:ldy #7
@byte:lda (uptr),y
 sta u_temp,y
 dey
 bpl @byte
 pla
 sta memac_bank_sel
 rts
.endproc
.proc u_render_bank
 jsr u_raw_cell
 lda memac_bank_sel
 pha
 lda #$80
 sta memac_bank_sel
 lda #0
 sta u_i
 sta uqptr
 lda #$ae
 sta uqptr+1
@row:
 ldx u_i
 lda u_temp,x
 sta u_bits
 cpx #7
 bne :+
 lda vstyle
 and #1
 beq :+
 lda #255
 sta u_bits
: lda #8
 sta u_j
 ldy #0
@pixel:
 asl u_bits
 lda #0
 sbc #0
 eor #255
 ldx bitmap_on
 cpx #2
 beq @store
 and #$f0
 sta u_byte
 asl u_bits
 lda #0
 sbc #0
 eor #255
 and #15
 ora u_byte
 dec u_j
@store:
 sta (uqptr),y
 iny
 dec u_j
 bne @pixel
 tya
 clc
 adc uqptr
 sta uqptr
 inc u_i
 lda u_i
 cmp #8
 bcc @row
 pla
 sta memac_bank_sel
 lda #0
 sta blit_cfg
 sta blit_cfg+2
 lda #$0e
 sta blit_cfg+1
 rts
.endproc
; Read character/style for a live cell selected by vrow/vcol.
.proc u_live_cell
 ldx vrow
 lda u_cell_lo,x
 sta uptr
 lda u_cell_hi,x
 sta uptr+1
 lda memac_bank_sel
 pha
 lda #$ff
 sta memac_bank_sel
 lda vcol
 asl
 tay
 lda (uptr),y
 sta vchar
 iny
 lda (uptr),y
 sta vattr
 pla
 sta memac_bank_sel
 ldx vrow
 lda u_style_lo,x
 sta uqptr
 lda u_style_hi,x
 sta uqptr+1
 ldy vcol
 lda (uqptr),y
 sta vstyle
 rts
.endproc
.proc u_combine_bank
 lda u_last_row
 sta vrow
 lda u_last_col
 sta vcol
 lda #0
 sta u_work
@cell:
 jsr u_live_cell
 jsr u_raw_cell
 lda memac_bank_sel
 pha
 lda vrow
 ldx vcol
 jsr u_pointer
 ldx u_work
 ldy #0
@bits:lda u_glyph,x
 ora u_temp,y
 sta (uptr),y
 inx
 inx
 iny
 cpy #8
 bcc @bits
 pla
 sta memac_bank_sel
 ldy vcol
 lda (uqptr),y
 ora #128
 sta (uqptr),y
 inc u_work
 inc vcol
 lda u_work
 cmp u_last_width
 bcc @cell
 rts
.endproc
.proc u_dcs_apply_bank
 lda memac_bank_sel
 pha
 lda u_dcs_buf
 ldx u_dcs_buf+1
 jsr u_pointer
 ldy #7
@bits:lda u_dcs_buf+4,y
 sta (uptr),y
 dey
 bpl @bits
 lda #$ff
 sta memac_bank_sel
 ldx u_dcs_buf
 lda u_cell_lo,x
 sta uptr
 lda u_cell_hi,x
 sta uptr+1
 lda u_dcs_buf+1
 asl
 tay
 lda #0
 sta (uptr),y
 iny
 lda u_dcs_buf+2
 sta (uptr),y
 lda u_style_lo,x
 sta uptr
 lda u_style_hi,x
 sta uptr+1
 ldy u_dcs_buf+1
 lda u_dcs_buf+3
 and #$13
 ora #128
 sta (uptr),y
 pla
 sta memac_bank_sel
 rts
.endproc
; Copy all 15,360 raw bytes. A/X are source/destination VRAM middle bytes.
.proc u_snapshot
 sta u_work
 stx u_work+1
 jsr b_blit_reset
 lda u_work
 sta blit_cfg+1
 cmp #$80
 lda #0
 adc #0
 eor #1
 sta blit_cfg+2
 lda u_work+1
 sta blit_cfg+7
 cmp #$80
 lda #0
 adc #0
 eor #1
 sta blit_cfg+8
 lda #1
 sta blit_cfg+4
 sta blit_cfg+10
 lda #255
 sta blit_cfg+12
 lda #59
 sta blit_cfg+14
 jmp b_blit
.endproc
.proc u_save_dialog_bank
 lda #$80
 ldx #$40
 jmp u_snapshot
.endproc
.proc u_restore_dialog_bank
 lda #$40
 ldx #$80
 jmp u_snapshot
.endproc
.proc u_alt_bank
 lda plus_count
 beq @restore
 lda #$80
 ldx #0
 jmp u_snapshot
@restore:
 lda #0
 ldx #$80
 jmp u_snapshot
.endproc
.proc u_scroll_up_bank
 lda sr_rows
 beq @out
 jsr b_blit_reset
 ldx sr_top
 lda u_raw_lo,x
 sta blit_cfg+6
 lda u_raw_hi,x
 sta blit_cfg+7
 txa
 clc
 adc sr_n
 tax
 lda u_raw_lo,x
 sta blit_cfg
 lda u_raw_hi,x
 sta blit_cfg+1
 jsr u_scroll_shape
@out:rts
.endproc
.proc u_scroll_down_bank
 lda sr_rows
 beq @out
 jsr b_blit_reset
 ldx sr_bot
 lda u_raw_lo,x
 clc
 adc #64
 sta blit_cfg+6
 lda u_raw_hi,x
 adc #1
 sta blit_cfg+7
 txa
 sec
 sbc sr_n
 tax
 lda u_raw_lo,x
 clc
 adc #64
 sta blit_cfg
 lda u_raw_hi,x
 adc #1
 sta blit_cfg+1
 jsr u_scroll_shape_fields
 lda #$c0
 sta blit_cfg+3
 sta blit_cfg+9
 lda #$fe
 sta blit_cfg+4
 sta blit_cfg+10
 jsr b_blit
@out:rts
.endproc
.proc u_scroll_shape
 jsr u_scroll_shape_fields
 jmp b_blit
.endproc
.proc u_scroll_shape_fields
 lda #64
 sta blit_cfg+3
 sta blit_cfg+9
 lda #1
 sta blit_cfg+4
 sta blit_cfg+10
 sta blit_cfg+13
 lda #63
 sta blit_cfg+12
 lda sr_rows
 asl
 sec
 sbc #1
 sta blit_cfg+14
 rts
.endproc
.proc u_history_address
 ; u_slot -> u_work[0:3] 24-bit address of 1024-byte history record.
 lda #0
 sta u_work
 lda u_slot
 asl
 rol u_work
 asl
 rol u_work
 sta u_work+1
 lda u_work
 clc
 adc #3
 sta u_work+2
 lda #0
 sta u_work
 rts
.endproc
.proc u_history_push_bank
 lda alternate
 jne @out
 lda history_head
 sta u_slot
 jsr u_history_address
 jsr b_blit_reset
 lda #$80
 sta blit_cfg+1
 ldx #2
@dst:lda u_work,x
 sta blit_cfg+6,x
 dex
 bpl @dst
 ; 640 bitmap bytes, two consecutive 320-byte scanlines.
 lda #64
 sta blit_cfg+3
 sta blit_cfg+9
 lda #1
 sta blit_cfg+4
 sta blit_cfg+10
 sta blit_cfg+13
 sta blit_cfg+14
 lda #63
 sta blit_cfg+12
 jsr b_blit
 jsr b_blit_reset
 lda #0
 sta blit_cfg
 lda #$f1
 sta blit_cfg+1
 lda #7
 sta blit_cfg+2
 lda #128
 sta blit_cfg+6
 lda u_work+1
 clc
 adc #2
 sta blit_cfg+7
 lda u_work+2
 sta blit_cfg+8
 lda #159
 sta blit_cfg+12
 jsr b_blit
 lda memac_bank_sel
 pha
 jsr u_history_style_ptr
 ldy #79
@styles:lda cell_styles,y
 sta (uptr),y
 dey
 bpl @styles
 pla
 sta memac_bank_sel
 inc history_head
 lda history_count+1
 bne @out
 inc history_count
 bne @out
 inc history_count+1
@out:rts
.endproc
.proc u_history_style_ptr
 lda u_work+1
 clc
 adc #3
 pha
 lsr
 lsr
 lsr
 lsr
 sta u_bank
 lda u_work+2
 asl
 asl
 asl
 asl
 ora u_bank
 ora #128
 sta memac_bank_sel
 pla
 and #15
 ora #$a0
 sta uptr+1
 lda #32
 sta uptr
 rts
.endproc
.proc u_history_draw_bank
 jsr b_scroll_page
 lda history_count
 sec
 sbc history_view
 sta history_index
 lda history_count+1
 sbc history_view+1
 sta history_index+1
 lda history_index
 sec
 sbc #24
 sta history_index
 lda history_index+1
 sbc #0
 sta history_index+1
 bcs :+
 lda #0
 sta history_index
 sta history_index+1
: lda #0
 sta u_histrow
@row:
 lda history_index+1
 cmp history_count+1
 bcc @copy
 jne @out
 lda history_index
 cmp history_count
 jcs @out
@copy:
 lda history_head
 sec
 sbc history_count
 clc
 adc history_index
 sta u_slot
 jsr u_history_address
 jsr b_blit_reset
 ldx #2
@src:lda u_work,x
 sta blit_cfg,x
 dex
 bpl @src
 ldx u_histrow
 lda u_raw_lo,x
 sta blit_cfg+6
 lda u_raw_hi,x
 sta blit_cfg+7
 lda #64
 sta blit_cfg+3
 sta blit_cfg+9
 lda #1
 sta blit_cfg+4
 sta blit_cfg+10
 sta blit_cfg+13
 sta blit_cfg+14
 lda #63
 sta blit_cfg+12
 jsr b_blit
 jsr b_blit_reset
 lda #128
 sta blit_cfg
 lda u_work+1
 clc
 adc #2
 sta blit_cfg+1
 lda u_work+2
 sta blit_cfg+2
 ldx u_histrow
 lda u_cell_lo,x
 sta blit_cfg+6
 lda u_cell_hi,x
 clc
 adc #$50
 sta blit_cfg+7
 lda #7
 sta blit_cfg+8
 lda #159
 sta blit_cfg+12
 jsr b_blit
 lda memac_bank_sel
 pha
 jsr u_history_style_ptr
 ldx u_histrow
 lda u_style_lo,x
 sta uqptr
 lda u_style_hi,x
 sta uqptr+1
 ldy #79
@styles:lda (uptr),y
 sta (uqptr),y
 dey
 bpl @styles
 pla
 sta memac_bank_sel
 inc history_index
 bne :+
 inc history_index+1
: inc u_histrow
 lda u_histrow
 cmp #24
 jcc @row
@out:rts
.endproc
u_raw_lo:.repeat 24,r
 .byte <($8000+r*640)
 .endrepeat
u_raw_hi:.repeat 24,r
 .byte >($8000+r*640)
 .endrepeat
u_style_lo:.repeat 24,r
 .byte <(cell_styles+r*80)
 .endrepeat
u_style_hi:.repeat 24,r
 .byte >(cell_styles+r*80)
 .endrepeat
u_cell_lo:.repeat 24,r
 .byte <($a100+r*160)
 .endrepeat
u_cell_hi:.repeat 24,r
 .byte >($a100+r*160)
 .endrepeat
