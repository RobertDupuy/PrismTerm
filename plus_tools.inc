; START opens the utility panel; OPTION retains the font/settings menu.
.proc plus_tools
 lda ui_ready
 beq :+
 lda #0
 jmp ui_open
:
 lda #1
 sta panel_open
 lda menu_active
 sta saved_menu_active
 lda #1
 sta menu_active
 ; Ask the remote to pause; no disk write is done during the menu itself.
 lda dev_ready
 beq :+
 lda #19
 jsr plus_queue
 jsr check_sendbuf
: lda #<tools_menu
 ldx #>tools_menu
 jsr menu_open
 lda saved_menu_active
 sta menu_active
 lda #0
 sta panel_open
 lda dev_ready
 beq :+
 lda #17
 jsr plus_queue
 jsr check_sendbuf
: rts
.endproc
tools_menu:
 .byte 0,0,28,23,20
 .word t_history,tool_history
 .word t_capture,tool_capture
 .word t_savecap,tool_save_capture
 .word t_saveprofile,tool_save_profile
 .word t_loadprofile,tool_load_profile
 .word t_macroedit,tool_edit_macro
 .word t_macro,tool_run_macro
 .word t_rx,tool_receive
 .word t_tx,tool_send
 .word t_yrx,tool_yreceive
 .word t_ytx,tool_ysend
 .word t_zrx,tool_zreceive
 .word t_zresume,tool_zresume
 .word t_ztx,tool_zsend
 .word t_krx,tool_kreceive
 .word t_ktx,tool_ksend
 .word t_utf8,tool_utf8
 .word t_status,tool_status
 .word t_disconnect,tool_disconnect
 .word t_quit,tool_quit
 t_history: .byte "Scrollback",0
 t_capture: .byte "Start/stop capture",0
 t_savecap: .byte "Save capture",0
 t_saveprofile: .byte "Save connection (1-8)",0
 t_loadprofile: .byte "Connect saved (1-8)",0
 t_macroedit: .byte "Edit login macro",0
 t_macro: .byte "Run login macro",0
 t_rx: .byte "Receive XMODEM-CRC",0
 t_tx: .byte "Send XMODEM-CRC",0
 t_yrx: .byte "Receive YMODEM",0
 t_ytx: .byte "Send YMODEM",0
 t_utf8: .byte "Toggle UTF-8 / CP437",0
 t_status: .byte "Session status",0
 t_disconnect: .byte "Disconnect",0
 t_quit: .byte "Exit to DOS",0
.proc tool_utf8
 lda utf8_mode
 eor #1
 sta utf8_mode
 lda #0
 sta utf_left
 lda #1
 sta menu_dismiss
 rts
.endproc
.proc tool_capture
 lda capture_on
 eor #1
 sta capture_on
 lda #1
 sta menu_dismiss
 rts
.endproc
.proc tool_run_macro
 lda #0
 sta macro_pos
 sta macro_delay
 lda #1
 sta macro_running
 sta menu_dismiss
 rts
.endproc
.proc tool_edit_macro
 jsr plus_dialog_begin
 lda #127
 sta input_limit
 lda #<macro_prompt
 ldx #>macro_prompt
 jsr plus_prompt
 lda input_cancel
 bne @end
 ldx #127
@copy:lda input_buf,x
 sta login_macro,x
 dex
 bpl @copy
@end: jmp plus_dialog_end
.endproc
macro_prompt: .byte "Macro (^M=RETURN, ~=1s pause):",13,10,$9b
filename_prompt: .byte "Atari filename (e.g. D:FILE.TXT): ",$9b
save_ok: .byte "Saved.",$9b
io_failed: .byte "Disk operation failed; data retained.",$9b
.proc tool_save_capture
 jsr plus_dialog_begin
 lda #63
 sta input_limit
 lda #<filename_prompt
 ldx #>filename_prompt
 jsr plus_prompt
 lda input_cancel
 jne @end
 lda #<input_buf
 ldx #>input_buf
 jsr plus_set_filename
 lda #8
 sta io_mode
 jsr plus_file_open
 lda io_error
 jne @error
 lda #0
 sta io_page
@page:lda io_page
 cmp capture_size+1
 bcc @full
 lda capture_size
 beq @closed
 sta io_size
 lda #0
 sta io_size+1
 jmp @copy
@full:lda #0
 sta io_size
 lda #1
 sta io_size+1
@copy:lda memac_bank_sel
 pha
 lda io_page
 and #15
 ora #$a0
 sta hptr+1
 lda #0
 sta hptr
 lda io_page
 lsr
 lsr
 lsr
 lsr
 ora #$f0
 sta memac_bank_sel
 ldy #0
@bytes:lda (hptr),y
 sta io_buffer,y
 iny
 bne @bytes
 pla
 sta memac_bank_sel
 lda #<io_buffer
 sta io_ptr
 lda #>io_buffer
 sta io_ptr+1
 jsr plus_file_write
 lda io_error
 bne @closeerror
 inc io_page
 lda io_page
 cmp capture_size+1
 bcc @page
 beq @page
@closed:jsr plus_file_close
 lda io_error
 bne @error
 lda #0
 sta capture_on
 sta capture_size
 sta capture_size+1
 sta capture_full
 lda #<save_ok
 ldx #>save_ok
 jsr plus_message
 jmp @end
@closeerror:jsr plus_file_close
@error:lda #<io_failed
 ldx #>io_failed
 jsr plus_message
@end: jmp plus_dialog_end
.endproc

.proc tool_history
 jsr plus_dialog_begin
 lda #0
 sta history_view
 sta history_view+1
@show:jsr history_draw
 jsr plus_key
 cmp #27
 beq @done
 cmp #13
 beq @done
 cmp #'-'
 beq @older
 cmp #'='
 beq @newer
 cmp #' '
 beq @older
 jmp @show
@older:
 lda history_view
 clc
 adc #24
 sta history_view
 bcc :+
 inc history_view+1
: lda history_view+1
 cmp history_count+1
 bcc @show
 bne @clamp
 lda history_view
 cmp history_count
 bcc @show
@clamp:lda history_count
 sta history_view
 lda history_count+1
 sta history_view+1
 jmp @show
@newer:
 lda history_view
 sec
 sbc #24
 sta history_view
 lda history_view+1
 sbc #0
 sta history_view+1
 bcs @show
 lda #0
 sta history_view
 sta history_view+1
 jmp @show
@done:jmp plus_dialog_end
.endproc
.proc history_draw
 lda u_active
 jne u_history_draw
 jsr scroll_page
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
 sta history_dst
@row:
 lda history_index+1
 cmp history_count+1
 bcc @copy
 bne @done
 lda history_index
 cmp history_count
 bcs @done
@copy:jsr plus_blit_reset
 lda history_head
 sec
 sbc history_count
 sta blit_cfg+1
 lda history_head+1
 sbc history_count+1
 sta blit_cfg+2
 lda blit_cfg+1
 clc
 adc history_index
 sta blit_cfg+1
 lda blit_cfg+2
 adc history_index+1
 and #3
 clc
 adc #3
 sta blit_cfg+2
 ldx history_dst
 lda row_addr_lo,x
 sta blit_cfg+6
 lda row_addr_hi,x
 sec
 sbc #$a0
 clc
 adc #$f0
 sta blit_cfg+7
 lda #7
 sta blit_cfg+8
 lda #159
 sta blit_cfg+12
 jsr plus_blit
 jsr history_styles
 inc history_index
 bne :+
 inc history_index+1
: inc history_dst
 lda history_dst
 cmp #24
 bcc @row
@done:jsr plus_dirty_all
 jmp plus_render_dirty
.endproc

.proc history_styles
 lda memac_bank_sel
 pha
 lda blit_cfg+2
 asl
 asl
 asl
 asl
 sta session_bank
 lda blit_cfg+1
 lsr
 lsr
 lsr
 lsr
 ora session_bank
 ora #128
 sta memac_bank_sel
 lda blit_cfg+1
 and #15
 ora #$a0
 sta hptr+1
 lda #160
 sta hptr
 ldx history_dst
 lda style_lo,x
 sta sptr
 lda style_hi,x
 sta sptr+1
 ldy #79
@copy: lda (hptr),y
 sta (sptr),y
 dey
 bpl @copy
 pla
 sta memac_bank_sel
 rts
.endproc

; Profile records occupy 512 bytes each at VRAM 1E000..1EFFF.
; name[32], URL[256], username[64], macro[128], protocol/UTF8/mode/width.
.proc profile_choose
 jsr profile_list
 lda #1
 sta input_limit
 lda #<slot_prompt
 ldx #>slot_prompt
 jsr plus_prompt
 lda input_cancel
 bne @bad
 lda input_buf
 cmp #'1'
 bcc @bad
 cmp #'9'
 bcs @bad
 sec
 sbc #'1'
 asl
 sta profile_slot
 clc
 rts
@bad:sec
 rts
.endproc
slot_prompt: .byte "Connection slot [1-8]: ",$9b
name_prompt: .byte "Connection name: ",$9b
.proc profile_list
 lda #0
 sta profile_slot
@slot: lda memac_bank_sel
 pha
 jsr profile_pointer
 ldy #31
@copy: lda (hptr),y
 sta input_buf,y
 dey
 bpl @copy
 pla
 sta memac_bank_sel
 lda #$9b
 sta input_buf+31
 lda profile_slot
 lsr
 clc
 adc #'1'
 sta temp_char
 jsr put_byte
 lda #':'
 sta temp_char
 jsr put_byte
 lda #' '
 sta temp_char
 jsr put_byte
 lda input_buf
 beq @empty
 lda #<input_buf
 ldx #>input_buf
 jsr print_str
 jmp @next
@empty: lda #<empty_slot
 ldx #>empty_slot
 jsr print_str
@next: jsr CR_adr
 jsr plus_index
 inc profile_slot
 inc profile_slot
 lda profile_slot
 cmp #16
 bcc @slot
 rts
.endproc
empty_slot: .byte "(empty)",$9b
.proc profile_pointer
 lda profile_slot
 clc
 adc #$a0
 sta hptr+1
 lda #0
 sta hptr
 lda #$9e
 sta memac_bank_sel
 rts
.endproc
.proc tool_save_profile
 jsr plus_dialog_begin
 jsr profile_choose
 jcs @end
 lda #31
 sta input_limit
 lda #<name_prompt
 ldx #>name_prompt
 jsr plus_prompt
 lda input_cancel
 jne @end
 lda #0
 tax
@zero: sta profile_rec,x
 sta profile_rec+256,x
 inx
 bne @zero
@name: lda input_buf,x
 sta profile_rec,x
 cmp #$9b
 beq @name_done
 inx
 cpx #32
 bcc @name
@name_done:
 ldx #0
@url:lda n_url_buf,x
 sta profile_rec+32,x
 inx
 bne @url
 ldx #63
@user:lda login_buf,x
 sta profile_rec+288,x
 dex
 bpl @user
 ldx #127
@macro:lda login_macro,x
 sta profile_rec+352,x
 dex
 bpl @macro
 lda proto_byte
 sta profile_rec+480
 lda dialog_core+(utf8_mode-plus_state_start)
 sta profile_rec+481
 lda dialog_atascii
 sta profile_rec+482
 lda term_last_col
 sta profile_rec+483
 lda active_font
 sta profile_rec+484
 ldx #3
@signature: lda profile_magic,x
 sta profile_rec+508,x
 dex
 bpl @signature
 lda memac_bank_sel
 pha
 jsr profile_pointer
 ldy #0
@copy1:lda profile_rec,y
 sta (hptr),y
 iny
 bne @copy1
 inc hptr+1
@copy2:lda profile_rec+256,y
 sta (hptr),y
 iny
 bne @copy2
 lda #<directory_path
 ldx #>directory_path
 jsr plus_set_filename
 lda #8
 sta io_mode
 jsr plus_file_open
 lda io_error
 bne @error
 lda #0
 sta io_ptr
 sta io_size
 lda #$a0
 sta io_ptr+1
 lda #16
 sta io_size+1
 jsr plus_file_write
 lda io_error
 pha
 jsr plus_file_close
 pla
 beq @error
 sta io_error
@error:pla
 sta memac_bank_sel
 lda io_error
 beq :+
 lda #<io_failed
 ldx #>io_failed
 jsr plus_message
 jmp @end
: lda #<save_ok
 ldx #>save_ok
 jsr plus_message
@end:jmp plus_dialog_end
.endproc
.proc tool_load_profile
 lda dev_ready
 beq :+
 rts
: jsr plus_dialog_begin
 jsr profile_choose
 jcs @end
 lda memac_bank_sel
 pha
 jsr profile_pointer
 ldy #0
@copy1:lda (hptr),y
 sta profile_rec,y
 iny
 bne @copy1
 inc hptr+1
@copy2:lda (hptr),y
 sta profile_rec+256,y
 iny
 bne @copy2
 pla
 sta memac_bank_sel
 ldx #3
@signature: lda profile_rec+508,x
 cmp profile_magic,x
 bne @end
 dex
 bpl @signature
 lda profile_rec+32
 cmp #'N'
 bne @end
 ldx #0
@url:lda profile_rec+32,x
 sta n_url_buf,x
 inx
 bne @url
 ldx #63
@user:lda profile_rec+288,x
 sta login_buf,x
 dex
 bpl @user
 ldx #127
@macro:lda profile_rec+352,x
 sta login_macro,x
 dex
 bpl @macro
 lda #$9b
 sta n_url_buf+255
 sta login_buf+63
 sta login_macro+127
 lda profile_rec+480
 sta proto_byte
 lda #1
 sta profile_ready
 sta menu_dismiss
@end:jmp plus_dialog_end
.endproc
profile_magic: .byte "VBX1"
.proc plus_print_number
 sta io_size
 stx io_size+1
 lda #0
 sta plus_count
@divide: lda #0
 ldy #16
@bit: asl io_size
 rol io_size+1
 rol
 cmp #10
 bcc :+
 sbc #10
 inc io_size
: dey
 bne @bit
 pha
 inc plus_count
 lda io_size
 ora io_size+1
 bne @divide
@digit: pla
 ora #'0'
 sta temp_char
 jsr put_byte
 dec plus_count
 bne @digit
 rts
.endproc
.proc tool_status
 jsr plus_dialog_begin
 lda #<status_title
 ldx #>status_title
 jsr print_str
 lda term_last_col
 clc
 adc #1
 ldx #0
 jsr plus_print_number
 lda #<status_columns
 ldx #>status_columns
 jsr print_str
 lda dialog_core+(utf8_mode-plus_state_start)
 beq @cp437
 lda #<status_utf8
 ldx #>status_utf8
 bne @encoding
@cp437: lda #<status_cp437
 ldx #>status_cp437
@encoding: jsr print_str
 lda #<status_history
 ldx #>status_history
 jsr print_str
 lda history_count
 ldx history_count+1
 jsr plus_print_number
 lda #<status_capture
 ldx #>status_capture
 jsr print_str
 lda capture_size
 ldx capture_size+1
 jsr plus_print_number
 lda #<status_bytes
 ldx #>status_bytes
 jsr print_str
 lda capture_on
 beq @stopped
 lda #<status_recording
 ldx #>status_recording
 bne @capture
@stopped: lda #<status_stopped
 ldx #>status_stopped
@capture: jsr print_str
 lda capture_full
 beq :+
 lda #<status_full
 ldx #>status_full
 jsr print_str
: lda #<press_key_msg
 ldx #>press_key_msg
 jsr print_str
 jsr plus_key
 jmp plus_dialog_end
.endproc
status_title: .byte "PrismTerm 0.3",13,10,13,10,"Display: ",$9b
status_columns: .byte " columns",13,10,"Encoding: ",$9b
status_utf8: .byte "UTF-8 / Unicode glyphs",$9b
status_cp437: .byte "CP437 / selected terminal mode",$9b
status_history: .byte 13,10,"Scrollback lines: ",$9b
status_capture: .byte 13,10,"Capture: ",$9b
status_bytes: .byte " bytes - ",$9b
status_recording: .byte "recording",$9b
status_stopped: .byte "stopped",$9b
status_full: .byte 13,10,"Capture full; save it to continue.",$9b
.proc tool_disconnect
 lda #1
 sta plus_quit
 sta menu_dismiss
 rts
.endproc
.proc tool_quit
 lda #2
 sta plus_quit
 lda #1
 sta menu_dismiss
 rts
.endproc

t_zrx: .byte "Receive ZMODEM",0
t_ztx: .byte "Send ZMODEM",0
t_krx: .byte "Receive Kermit",0
t_ktx: .byte "Send Kermit",0
tool_zreceive:
 lda #2
 sta xmode
 lda #0
 sta xdirection
 jmp transfer_dialog
tool_zsend:
 lda #2
 sta xmode
 lda #1
 sta xdirection
 jmp transfer_dialog
tool_kreceive:
 lda #3
 sta xmode
 lda #0
 sta xdirection
 jmp transfer_dialog
tool_ksend:
 lda #3
 sta xmode
 lda #1
 sta xdirection
 jmp transfer_dialog

t_zresume: .byte "Resume ZMODEM receive",0
tool_zresume:
 lda #4
 sta xmode
 lda #0
 sta xdirection
 jmp transfer_dialog
