; Local UI bank: VRAM $01000..$03fff, mapped at CPU $5000..$7fff.
; Palette 2 and one saved screen keep all 24 terminal rows intact on return.
.assert __BSS_RUN__ >= $8000, lderror, "Shared UI state must stay outside MEMAC B"
.segment "RESIDENT"
ui_ready: .byte 0
serial_baud_code: .byte 14
ui_category: .byte 0
ui_selected: .byte 0
ui_page: .byte 0
ui_action: .byte 0
ui_cmd: .byte 0
ui_arg: .byte 0
ui_x: .byte 0
ui_y: .byte 0
ui_color: .byte $cf
ui_width: .byte 80
ui_count: .byte 0
ui_index: .byte 0
ui_key_value: .byte 0
ui_old_font_mode: .byte 0
ui_quick: .byte 0
ui_profile_mode: .byte 0
ui_field: .byte 0
ui_resume: .byte 0
ui_protocol: .byte 0
ui_xfer_active: .byte 0
ui_rx_bytes: .res 4,0
ui_tx_bytes: .res 4,0
ui_progress_tick: .byte 0
ui_number: .res 4,0
ui_digits: .res 11,0
ui_edit_ptr: .word 0
ui_edit_max: .byte 0
ui_edit_y: .byte 0
ui_edit_mask: .byte 0
ui_edit_len: .byte 0
ui_edit_offset: .byte 0
ui_saved_p: .byte 0
ui_error: .byte 0
ui_save_slot: .byte 0

.proc ui_load
 lda #0
 sta ui_ready
 lda #14
 sta serial_baud_code
 lda #<ui_path
 ldx #>ui_path
 jsr plus_set_filename
 lda #4
 sta io_mode
 jsr plus_file_open
 lda io_error
 bne @out
 lda memac_bank_sel
 pha
 lda #$81
 sta memac_bank_sel
@page:
 lda #0
 sta io_ptr
 sta io_size
 lda #$a0
 sta io_ptr+1
 lda #16
 sta io_size+1
 jsr plus_file_read
 lda io_error
 bne @close
 inc memac_bank_sel
 lda memac_bank_sel
 cmp #$84
 bne @page
 inc ui_ready
@close:
 pla
 sta memac_bank_sel
 jsr plus_file_close
 ; Keep the startup CP437 font for menus opened from ATASCII mode.
 lda #$c0
 ldx #0
 jsr ui_font_copy
@out:rts
.endproc
ui_path: .byte "D:UI.BIN",$9b

; A=destination middle byte, X=source middle byte (both in first 64 KiB).
.proc ui_font_copy
 pha
 txa
 pha
 jsr plus_blit_reset
 pla
 sta blit_cfg+1
 pla
 sta blit_cfg+7
 lda #1
 sta blit_cfg+4
 sta blit_cfg+10
 lda #255
 sta blit_cfg+12
 lda #7
 sta blit_cfg+14
 jmp plus_blit
.endproc
.proc ui_enter
 sta proto_entry
 stx proto_entry+1
 lda #$80
 jmp bank_enter
.endproc

.proc ui_open
 sta ui_category
 lda menu_active
 sta saved_menu_active
 lda #1
 sta menu_active
 sta panel_open
 lda #0
 sta menu_key_ready
 sta ui_page
 sta ui_selected
 sta ui_action
@release:
 lda CONSOL
 and #5
 cmp #5
 bne @release
 lda dev_ready
 beq :+
 lda #19
 jsr plus_queue
 jsr check_sendbuf
: jsr plus_dialog_begin
 lda #$c8
 ldx #0
 jsr ui_font_copy
 lda dialog_atascii
 beq :+
 lda #0
 ldx #$c0
 jsr ui_font_copy
 jsr plus_font_changed
: lda #0
 sta dialog_font_atascii
 lda #<ui_loop
 ldx #>ui_loop
 jsr ui_enter
 lda #0
 ldx #$c8
 jsr ui_font_copy
 lda dialog_atascii
 beq :+
 jsr plus_font_changed
: lda memac_bank_sel
 pha
 lda #$80
 sta memac_bank_sel
 lda #$11
 sta $a803
 pla
 sta memac_bank_sel
 jsr plus_dialog_end
 lda ui_action
 cmp #1
 bne :+
 jsr tool_history
: lda saved_menu_active
 sta menu_active
 lda #0
 sta panel_open
 sta menu_key_ready
 lda dev_ready
 beq @out
 lda #17
 jsr plus_queue
 jsr check_sendbuf
@out:rts
.endproc

.proc ui_host_command
 lda ui_cmd
 cmp #1
 beq @font
 cmp #2
 beq @mode
 cmp #3
 beq @width
 cmp #4
 beq @baud
 cmp #5
 beq @transfer
 rts
@font:
 lda ui_arg
 sta active_font
 jsr font_load_index
 lda #0
 sta dialog_atascii
 jmp @save_font
@mode:
 lda dialog_atascii
 eor #1
 sta dialog_atascii
 beq @ansi
 jsr _vbxe_font_from_rom
 lda #39
 sta term_last_col
 bne @save_font
@ansi:
 lda active_font
 jsr font_load_index
 lda #79
 sta term_last_col
@save_font:
 lda #$c8
 ldx #0
 jsr ui_font_copy
 lda dialog_atascii
 beq @font_done
 lda #0
 ldx #$c0
 jsr ui_font_copy
@font_done:
 jsr plus_font_changed
 jmp plus_enable_bitmap
@width:
 lda term_last_col
 eor #104
 sta term_last_col
 jsr plus_enable_bitmap
 jmp telnet_naws
@baud:
 jsr font_swap_prep_r
 lda ui_arg
 sta serial_baud_code
 jmp font_swap_restore_r
@transfer:
 lda #17
 jsr plus_queue
 jsr check_sendbuf
 lda xmode
 cmp #2
 beq @z
 cmp #4
 beq @z
 cmp #3
 beq @k
 lda xdirection
 beq :+
 jmp xfer_send
: jmp xfer_receive
@z: lda xdirection
 beq :+
 jmp zmodem_send
: jmp zmodem_receive
@k: lda xdirection
 beq :+
 jmp kermit_send
: jmp kermit_receive
.endproc

.proc ui_render
 jsr plus_dirty_all
 jmp plus_render_dirty
.endproc
.proc ui_backdrop
 jsr plus_blit_reset
 lda #$f1
 sta blit_cfg+7
 lda #7
 sta blit_cfg+8
 lda #7
 ldx #$a0
 jmp plus_copy_screen
.endproc
.proc ui_rx_progress
 lda ui_xfer_active
 beq @out
 inc ui_rx_bytes
 bne ui_progress_update
 inc ui_rx_bytes+1
 bne ui_progress_update
 inc ui_rx_bytes+2
 bne ui_progress_update
 inc ui_rx_bytes+3
 jmp ui_progress_update
@out:rts
.endproc
.proc ui_tx_progress
 lda ui_xfer_active
 beq @out
 clc
 lda ui_tx_bytes
 adc io_size
 sta ui_tx_bytes
 lda ui_tx_bytes+1
 adc io_size+1
 sta ui_tx_bytes+1
 bcc ui_progress_update
 inc ui_tx_bytes+2
 bne ui_progress_update
 inc ui_tx_bytes+3
 jmp ui_progress_update
@out:rts
.endproc
.proc ui_progress_update
 lda $14
 sec
 sbc ui_progress_tick
 cmp #30
 bcc @out
 lda $14
 sta ui_progress_tick
 txa
 pha
 tya
 pha
 lda #<ui_progress_draw
 ldx #>ui_progress_draw
 jsr ui_enter
 pla
 tay
 pla
 tax
@out:rts
.endproc
gate ui_key,plus_key
gate ui_refresh,ui_render
gate ui_background,ui_backdrop
gate ui_command,ui_host_command
gate ui_file_open,plus_file_open
gate ui_file_read,plus_file_read
gate ui_file_write,plus_file_write
gate ui_file_close,plus_file_close
gate ui_set_filename,plus_set_filename
gate ui_naws,telnet_naws
.segment "CODE"
