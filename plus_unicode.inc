; Native Unicode pixels and full-layout DCS bridge. Wide/combining glyphs do
; not depend on a finite screen character set: pixels belong to each cell.
uptr=$d0
uqptr=$d2
.segment "BSS"
u_active:.res 1
u_override:.res 1
u_flags:.res 1
u_glyph:.res 16
u_last_row:.res 1
u_last_col:.res 1
u_last_width:.res 1
u_argrow:.res 1
u_argcol:.res 1
u_argdst:.res 1
u_command:.res 1
u_temp:.res 8
u_dcs_stage:.res 1
u_dcs_len:.res 1
u_dcs_half:.res 1
u_dcs_buf:.res 14
u_sector:.res 256
u_directory:.res 256
u_dir_page:.res 1
u_disk_ok:.res 1
u_disk_error:.res 1
u_font_unit:.res 1
u_sector_no:.res 2
u_cached_sector:.res 2
.segment "RESIDENT"
.macro ucall label,entry
.proc label
 lda u_active
 beq @out
 lda #<entry
 ldx #>entry
 jmp proto_enter
@out:rts
.endproc
.endmacro
ucall u_render_source,u_render_bank
ucall u_scroll_up,u_scroll_up_bank
ucall u_scroll_down,u_scroll_down_bank
ucall u_save_dialog,u_save_dialog_bank
ucall u_restore_dialog,u_restore_dialog_bank
ucall u_alt_snapshot,u_alt_bank
.proc u_activate
 lda u_active
 bne @out
 jsr plus_enable_bitmap
 lda #<u_init_bank
 ldx #>u_init_bank
 jsr proto_enter
 lda #1
 sta u_active
 lda #0
 sta u_last_width
 sta history_head
 sta history_head+1
 sta history_count
 sta history_count+1
 lda #2
 sta u_font_unit
 lda #255
 sta u_dir_page
@out:rts
.endproc
; A row, X column -> MEMAC A and (uptr). Raw cell rows are 640 bytes.
.proc u_cell_ptr
 sta u_argrow
 stx u_argcol
 lda memac_bank_sel
 pha
 lda #<u_ptr_bank
 ldx #>u_ptr_bank
 jsr proto_enter
 pla
 ; Caller restores the original MEMAC A mapping after copying.
 rts
.endproc
; Local, resident address arithmetic avoids a bank call for every cell.
.proc u_pointer
 sta u_argrow
 txa
 asl
 sta uptr
 lda #0
 rol
 sta uptr+1
 asl uptr
 rol uptr+1
 asl uptr
 rol uptr+1
 ldx u_argrow
 lda u_res_raw_lo,x
 clc
 adc uptr
 sta uptr
 lda u_res_raw_hi,x
 adc uptr+1
 sta uptr+1
 lsr
 lsr
 lsr
 lsr
 ora #$80
 sta memac_bank_sel
 lda uptr+1
 and #15
 ora #$a0
 sta uptr+1
 rts
.endproc
.proc u_hook
 lda u_active
 beq @out
 lda u_override
 bne @custom
 lda row
 sta u_last_row
 lda column
 sta u_last_col
 lda #1
 sta u_last_width
 rts
@custom:
 lda memac_bank_sel
 pha
 lda row
 ldx column
 jsr u_pointer
 lda u_override
 sec
 sbc #1
 tax
 ldy #0
@copy:lda u_glyph,x
 sta (uptr),y
 inx
 inx
 iny
 cpy #8
 bcc @copy
 pla
 sta memac_bank_sel
 jsr plus_style_ptr
 ldy #0
 lda (sptr),y
 ora #128
 sta (sptr),y
@out:rts
.endproc
.proc u_emit
 lda proto_ready
 bne :+
 lda #'?'
 sta temp_char
 jmp plus_put
: jsr u_activate
 lda #<u_lookup_bank
 ldx #>u_lookup_bank
 jsr proto_enter
 lda u_flags
 and #3
 beq @combine
 sta u_last_width
 lda wrap_pending
 beq :+
 lda #0
 sta column
 jsr plus_index
: lda u_last_width
 cmp #2
 bne @write
 lda column
 cmp term_last_col
 bcc @write
 lda wrap_disabled
 bne @single
 lda #0
 sta column
 jsr plus_index
 jmp @write
@single:lda #1
 sta u_last_width
@write:
 lda row
 sta u_last_row
 lda column
 sta u_last_col
 lda #1
 sta u_override
 lda #'?'
 sta temp_char
 jsr plus_put
 lda u_last_width
 cmp #2
 bne @done
 lda #2
 sta u_override
 lda #' '
 sta temp_char
 jsr plus_put
@done:lda #0
 sta u_override
 rts
@combine:
 lda u_last_width
 bne @attach
 lda column
 ldx wrap_pending
 bne :+
 cmp #0
 beq @done
 sec
 sbc #1
: sta u_last_col
 lda row
 sta u_last_row
 lda #1
 sta u_last_width
@attach:
 lda #<u_combine_bank
 ldx #>u_combine_bank
 jsr proto_enter
 jmp plus_dirty_all
.endproc
.proc u_copy_cell
 lda u_active
 beq @out
 lda memac_bank_sel
 pha
 lda row
 ldx u_argcol
 jsr u_pointer
 ldy #7
@read:lda (uptr),y
 sta u_temp,y
 dey
 bpl @read
 lda row
 ldx u_argdst
 jsr u_pointer
 ldy #7
@write:lda u_temp,y
 sta (uptr),y
 dey
 bpl @write
 pla
 sta memac_bank_sel
@out:rts
.endproc
.proc u_insert_cell
 lda vcol
 sta u_argcol
 clc
 adc plus_count
 sta u_argdst
 jmp u_copy_cell
.endproc
.proc u_delete_cell
 lda vbyte
 sta u_argcol
 lda vcol
 sta u_argdst
 jmp u_copy_cell
.endproc
.proc u_history_push
 lda #<u_history_push_bank
 ldx #>u_history_push_bank
 jmp proto_enter
.endproc
.proc u_history_draw
 lda #<u_history_draw_bank
 ldx #>u_history_draw_bank
 jsr proto_enter
 jsr plus_dirty_all
 jmp plus_render_dirty
.endproc
; Raw font disk (default D2), 256-byte sectors. SIO does not use a DOS file
; handle or seek API. ROM/driver mappings are restored by the resident gate.
.proc u_disk_read
 lda u_sector_no
 cmp u_cached_sector
 bne @read
 lda u_sector_no+1
 cmp u_cached_sector+1
 bne @read
 lda u_disk_error
 bmi @read
 rts
@read: jsr font_swap_prep_r
 lda #$31
 sta DDEVIC
 lda u_font_unit
 sta DUNIT
 lda #'R'
 sta DCOMND
 lda #$40
 sta DSTATS
 lda #<u_sector
 sta DBUFLO
 lda #>u_sector
 sta DBUFHI
 lda #0
 sta DBYTLO
 lda #1
 sta DBYTHI
 lda u_sector_no+1
 bne :+
 lda u_sector_no
 cmp #4
 bcs :+
 lda #128
 sta DBYTLO
 lda #0
 sta DBYTHI
: lda u_sector_no
 sta DAUX1
 lda u_sector_no+1
 sta DAUX2
 lda #7
 sta DTIMLO
 jsr SIOV
 lda u_sector_no
 sta u_cached_sector
 lda u_sector_no+1
 sta u_cached_sector+1
 lda DSTATS
 sta u_disk_error
 jsr font_swap_restore_r
 rts
.endproc
; DCS =U + 28 hex digits + ST. CRC16 protects row, column, colors and eight
; bitmap bytes. No effect until the complete bounded packet has been verified.
.proc u_dcs_start
 lda #7
 sta ctrl_seq_flg
 lda #0
 sta u_dcs_stage
 sta u_dcs_len
 sta u_dcs_half
 rts
.endproc
.proc u_dcs_process
 lda temp_char
 cmp #27
 jeq @esc
 ldx u_dcs_stage
 cpx #3
 jeq @finish
 cpx #0
 beq @equals
 cpx #1
 beq @tag
 lda u_dcs_len
 cmp #14
 jcs @ignore
 lda temp_char
 cmp #'0'
 jcc @ignore
 cmp #'9'+1
 bcc @decimal
 ora #32
 cmp #'a'
 jcc @ignore
 cmp #'f'+1
 jcs @ignore
 sec
 sbc #'a'-10
 jmp @hex
@decimal:sec
 sbc #'0'
@hex:
 ldx u_dcs_half
 bne @low
 asl
 asl
 asl
 asl
 sta u_dcs_half
 inc u_dcs_half
 rts
@low:
 sta u_dcs_buf+13 ; scratch until the last byte is complete
 lda u_dcs_half
 sec
 sbc #1
 ora u_dcs_buf+13
 ldx u_dcs_len
 sta u_dcs_buf,x
 inc u_dcs_len
 lda #0
 sta u_dcs_half
 rts
@equals:lda temp_char
 cmp #'='
 bne @ignore
 inc u_dcs_stage
 rts
@tag:lda temp_char
 cmp #'U'
 bne @ignore
 inc u_dcs_stage
 rts
@esc:lda #3
 sta u_dcs_stage
 rts
@ignore:lda #3
 sta ctrl_seq_flg
 rts
@finish:
 lda proto_ready
 beq @ignore
 lda #0
 sta ctrl_seq_flg
 lda temp_char
 cmp #'\'
 bne @out
 lda u_dcs_len
 cmp #14
 bne @out
 lda u_dcs_half
 bne @out
 lda u_dcs_buf
 cmp #24
 bcs @out
 lda u_dcs_buf+1
 cmp #80
 bcs @out
 lda #0
 sta xcrc
 sta xcrc+1
 sta u_dcs_len
@crc:ldx u_dcs_len
 lda u_dcs_buf,x
 jsr xcrc_byte
 inc u_dcs_len
 lda u_dcs_len
 cmp #12
 bcc @crc
 lda xcrc+1
 cmp u_dcs_buf+12
 bne @out
 lda xcrc
 cmp u_dcs_buf+13
 bne @out
 jsr u_activate
 lda #<u_dcs_apply_bank
 ldx #>u_dcs_apply_bank
 jsr proto_enter
 ldx u_dcs_buf
 lda u_dcs_buf+1
 jmp plus_dirty_cell
@out:rts
.endproc
u_res_raw_lo:.repeat 24,r
 .byte <($8000+r*640)
 .endrepeat
u_res_raw_hi:.repeat 24,r
 .byte >($8000+r*640)
 .endrepeat
.segment "CODE"
