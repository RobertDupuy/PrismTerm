; Session state in VBXE RAM: history 30000..6FFFF (1024 rows), capture
; 70000..77FFF (32 KiB), dialog 7A000, alternate screen 7C000.
hptr=$c8
.segment "BSS"
alt_scroll_top: .res 1
alt_scroll_bot: .res 1
history_head: .res 2
history_count: .res 2
history_view: .res 2
history_index: .res 2
history_dst: .res 1
capture_on: .res 1
capture_size: .res 2
capture_full: .res 1
session_bank: .res 1
io_error: .res 1
io_mode: .res 1
io_name: .res 64
io_buffer: .res 512
io_size: .res 2
io_ptr: .res 2
io_page: .res 1
profile_slot: .res 1
profile_ready: .res 1
profile_rec: .res 512
login_macro: .res 128
macro_pos: .res 1
macro_running: .res 1
macro_delay: .res 1
input_limit: .res 1
input_len: .res 1
input_buf: .res 128
input_cancel: .res 1
saved_menu_active: .res 1
dialog_row: .res 1
dialog_col: .res 1
dialog_color: .res 1
dialog_wrap: .res 1
dialog_fg: .res 1
dialog_bg: .res 1
dialog_style: .res 1
dialog_core: .res plus_state_end-plus_state_start
dialog_atascii: .res 1
dialog_font_atascii: .res 1
dialog_top: .res 1
dialog_bot: .res 1
panel_open: .res 1
panel_tick: .res 1
plus_quit: .res 1
.segment "CODE"
.proc plus_session_init
 lda #0
 sta history_head
 sta history_head+1
 sta history_count
 sta history_count+1
 sta capture_size
 sta capture_size+1
 sta capture_on
 sta capture_full
 sta macro_running
 sta macro_delay
 sta login_macro
 sta profile_ready
 sta panel_open
 sta plus_quit
 jsr plus_video_init
 ; Initialize directory RAM; missing directory is a normal first boot.
 lda memac_bank_sel
 pha
 lda #$9e
 sta memac_bank_sel
 lda #0
 sta hptr
 lda #$a0
 sta hptr+1
 ldx #16
 lda #0
 ldy #0
@clear: sta (hptr),y
 iny
 bne @clear
 inc hptr+1
 dex
 bne @clear
 lda #<directory_path
 ldx #>directory_path
 jsr plus_set_filename
 lda #4
 sta io_mode
 jsr plus_file_open
 lda io_error
 bne @end
 lda #0
 sta io_ptr
 sta io_size
 lda #$a0
 sta io_ptr+1
 lda #16
 sta io_size+1
 jsr plus_file_read
 jsr plus_file_close
@end: pla
 sta memac_bank_sel
 jsr proto_load
 jmp ui_load
.endproc

.proc plus_history_clear
 lda #0
 sta history_head
 sta history_head+1
 sta history_count
 sta history_count+1
 rts
.endproc
.proc plus_history_push
 lda u_active
 jne u_history_push
 lda alternate
 jne @ret
 jsr plus_blit_reset
 lda #0
 sta blit_cfg
 lda #$f1
 sta blit_cfg+1
 lda #7
 sta blit_cfg+2
 lda history_head
 sta blit_cfg+7
 lda history_head+1
 clc
 adc #3
 sta blit_cfg+8
 lda #159
 sta blit_cfg+12
 jsr plus_blit
 ; metadata occupies bytes 160..239 of the slot; copy only when needed.
 lda memac_bank_sel
 pha
 lda history_head
 and #15
 clc
 adc #$a0
 sta hptr+1
 lda #160
 sta hptr
 lda history_head+1
 asl
 asl
 asl
 asl
 sta session_bank
 lda history_head
 lsr
 lsr
 lsr
 lsr
 ora session_bank
 clc
 adc #$b0
 sta memac_bank_sel
 ldy #79
@style: lda #0
 ldx bitmap_on
 beq :+
 lda cell_styles,y
: sta (hptr),y
 dey
 bpl @style
 pla
 sta memac_bank_sel
@increment:
 inc history_head
 bne :+
 inc history_head+1
 lda history_head+1
 and #3
 sta history_head+1
: lda history_count+1
 cmp #4
 bcs @ret
 inc history_count
 bne @ret
 inc history_count+1
@ret: rts
.endproc

; Save/restore full cell page at source/destination bank configured in A/X.
; A = source high byte in 19-bit address, X = middle byte. dest already set.
.proc plus_copy_screen
 sta blit_cfg+2
 stx blit_cfg+1
 lda #0
 sta blit_cfg
 sta blit_cfg+6
 sta blit_cfg+3
 sta blit_cfg+9
 lda #1
 sta blit_cfg+4
 sta blit_cfg+10
 lda #255
 sta blit_cfg+12
 lda #14
 sta blit_cfg+14
 jmp plus_blit
.endproc
.proc plus_alternate
 lda plus_count
 cmp alternate
 jeq @ret
 jsr cursor_off
 jsr u_alt_snapshot
 lda plus_count
 beq @restore
 lda scroll_top
 sta alt_scroll_top
 lda scroll_bot
 sta alt_scroll_bot
 lda #$fd
 ldx #0
 jsr plus_styles_snapshot
 jsr plus_save
 ldx #11
@state: lda saved_state,x
 sta alt_saved_state,x
 dex
 bpl @state
 jsr plus_blit_reset
 lda #$c0
 sta blit_cfg+7
 lda #7
 sta blit_cfg+8
 lda #7
 ldx #$f1
 jsr plus_copy_screen
 lda #1
 sta alternate
 lda #0
 sta row
 sta column
 jsr scroll_page
 jsr reset_margins
 jsr plus_dirty_all
 jmp plus_position
@restore:
 lda alt_scroll_top
 sta scroll_top
 lda alt_scroll_bot
 sta scroll_bot
 lda #$fd
 ldx #1
 jsr plus_styles_snapshot
 jsr plus_blit_reset
 lda #$f1
 sta blit_cfg+7
 lda #7
 sta blit_cfg+8
 lda #7
 ldx #$c0
 jsr plus_copy_screen
 ldx #11
@load: lda alt_saved_state,x
 sta saved_state,x
 dex
 bpl @load
 lda #0
 sta alternate
 jsr plus_dirty_all
 jmp plus_restore
@ret: rts
.endproc

.proc plus_capture_byte
 lda capture_on
 beq @ret
 lda capture_size+1
 cmp #128
 bcs @full
 lda memac_bank_sel
 pha
 lda capture_size
 sta hptr
 lda capture_size+1
 and #15
 ora #$a0
 sta hptr+1
 lda capture_size+1
 lsr
 lsr
 lsr
 lsr
 ora #$f0
 sta memac_bank_sel
 lda temp_char
 ldy #0
 sta (hptr),y
 pla
 sta memac_bank_sel
 inc capture_size
 bne @ret
 inc capture_size+1
@ret: rts
@full: lda #0
 sta capture_on
 lda #1
 sta capture_full
 rts
.endproc

.proc plus_idle
 lda $14
 cmp last_tick
 beq @render
 sta last_tick
 lda bell_ticks
 beq :+
 dec bell_ticks
 lda bell_ticks
 and #1
 asl
 asl
 asl
 ora #7
 sta CONSOL
: lda bitmap_on
 beq @macro_time
 inc blink_clock
 lda blink_clock
 cmp #30
 bcc @macro_time
 lda #0
 sta blink_clock
 lda blink_phase
 eor #1
 sta blink_phase
 ldx #23
@blink: lda blink_rows,x
 beq :+
 jsr plus_blink_row
: dex
 bpl @blink
@macro_time: lda macro_delay
 beq :+
 dec macro_delay
: jsr plus_macro_pump
@render:jsr cursor_on
 jsr plus_render_dirty
 lda CONSOL
 and #1
 bne @ret
 lda panel_open
 bne @ret
 jmp plus_tools
@ret: rts
.endproc

.proc plus_macro_pump
 lda macro_running
 beq @ret
 lda macro_delay
 bne @ret
 ldy macro_pos
 lda login_macro,y
 beq @done
 cmp #$9b
 beq @done
 inc macro_pos
 cmp #'~'
 beq @delay
 cmp #'^'
 bne @send
 ldy macro_pos
 lda login_macro,y
 beq @done
 inc macro_pos
 and #31
@send: jsr plus_queue
@ret: rts
@delay:lda #60
 sta macro_delay
 rts
@done:lda #0
 sta macro_running
 rts
.endproc

; Disk I/O uses IOCB 4. Suspend concurrent R: before disk SIO and reopen
; it afterwards. Stop-and-wait transfers commit before acknowledging. A
; streaming ZMODEM sender also depends on the transport's buffering/flow control.
.proc plus_set_filename
 sta hptr
 stx hptr+1
 ldy #0
@copy: lda (hptr),y
 sta io_name,y
 cmp #$9b
 beq @ret
 iny
 cpy #63
 bcc @copy
 lda #$9b
 sta io_name+63
@ret: rts
.endproc
.proc plus_file_open
 jsr font_swap_prep_r
 ldx #$40
 lda #12
 sta ICCOM+$40
 jsr CIOV
 ldx #$40
 lda #3
 sta ICCOM+$40
 lda #<io_name
 sta ICBA+$40
 lda #>io_name
 sta ICBA+$41
 lda io_mode
 sta ICAX1+$40
 lda #0
 sta ICAX2+$40
 jsr CIOV
 sty io_error
 jsr font_swap_restore_r
 lda io_error
 bmi @ret
 lda #0
 sta io_error
@ret: rts
.endproc
.proc plus_file_read
 lda #7
 bne plus_file_io
.endproc
.proc plus_file_write
 lda #11
 bne plus_file_io
.endproc
plus_file_io:
 pha
 jsr font_swap_prep_r
 pla
 sta ICCOM+$40
 lda io_ptr
 sta ICBA+$40
 lda io_ptr+1
 sta ICBA+$41
 lda io_size
 sta ICBL+$40
 lda io_size+1
 sta ICBL+$41
 ldx #$40
 jsr CIOV
 sty io_error
 lda ICBL+$40
 sta io_size
 lda ICBL+$41
 sta io_size+1
 jsr font_swap_restore_r
 lda io_error
 bmi :+
 lda #0
 sta io_error
: rts
.proc plus_file_close
 jsr font_swap_prep_r
 ldx #$40
 lda #12
 sta ICCOM+$40
 jsr CIOV
 sty io_error
 jsr font_swap_restore_r
 lda io_error
 bmi :+
 lda #0
 sta io_error
: rts
.endproc

.proc plus_dialog_begin
 jsr u_save_dialog
 jsr cursor_off
 ldx #0
@save_core: lda plus_state_start,x
 sta dialog_core,x
 inx
 cpx #plus_state_end-plus_state_start
 bne @save_core
 lda #$fb
 ldx #0
 jsr plus_styles_snapshot
 lda row
 sta dialog_row
 lda column
 sta dialog_col
 lda wrap_pending
 sta dialog_wrap
 lda text_color
 sta dialog_color
 lda plus_fg
 sta dialog_fg
 lda plus_bg
 sta dialog_bg
 lda plus_style
 sta dialog_style
 jsr plus_blit_reset
 lda #$a0
 sta blit_cfg+7
 lda #7
 sta blit_cfg+8
 lda #7
 ldx #$f1
 jsr plus_copy_screen
 lda atascii_mode
 sta dialog_atascii
 sta dialog_font_atascii
 lda scroll_top
 sta dialog_top
 lda scroll_bot
 sta dialog_bot
 jsr reset_margins
 lda #0
 sta atascii_mode
 sta plus_inverse
 sta plus_bold
 sta g0
 sta g1
 sta gsel
 sta insert_mode
 sta utf8_mode
 sta origin_mode
 sta wrap_disabled
 sta cursor_hidden
 sta row
 sta column
 sta wrap_pending
 sta plus_bg
 sta plus_style
 lda #7
 sta plus_fg
 jsr plus_color
 jsr scroll_page
 jsr plus_dirty_all
 jmp recalc_cursor
.endproc
.proc plus_dialog_end
 jsr u_restore_dialog
 jsr cursor_off
 lda #$fb
 ldx #1
 jsr plus_styles_snapshot
 jsr plus_blit_reset
 lda #$f1
 sta blit_cfg+7
 lda #7
 sta blit_cfg+8
 lda #7
 ldx #$a0
 jsr plus_copy_screen
 ldx #0
@restore_core: lda dialog_core,x
 sta plus_state_start,x
 inx
 cpx #plus_state_end-plus_state_start
 bne @restore_core
 lda #0
 sta dialog_font_atascii
 lda dialog_atascii
 sta atascii_mode
 lda dialog_top
 sta scroll_top
 lda dialog_bot
 sta scroll_bot
 lda dialog_row
 sta row
 lda dialog_col
 sta column
 lda dialog_color
 sta text_color
 lda dialog_fg
 sta plus_fg
 lda dialog_bg
 sta plus_bg
 lda dialog_style
 sta plus_style
 jsr plus_color
 jsr plus_dirty_all
 jsr plus_position
 lda dialog_wrap
 sta wrap_pending
 rts
.endproc
.proc plus_key
 jsr plus_render_dirty
@wait: lda menu_key_ready
 beq @wait
 lda #0
 sta menu_key_ready
 ldx menu_key
 lda keycode_table,x
 rts
.endproc
.proc plus_prompt
 ; A/X = EOL-terminated prompt. Returns input_buf EOL terminated.
 jsr print_str
 lda #0
 sta input_len
 sta input_cancel
@key: jsr plus_key
 cmp #13
 beq @done
 cmp #27
 beq @cancel
 cmp #8
 beq @back
 cmp #127
 beq @back
 cmp #126
 beq @back
 cmp #32
 bcc @key
 cmp #127
 bcs @key
 ldy input_len
 cpy input_limit
 bcs @key
 sta input_buf,y
 inc input_len
 sta temp_char
 jsr put_byte
 jmp @key
@back:lda input_len
 beq @key
 dec input_len
 jsr BS_adr
 lda #' '
 sta temp_char
 jsr put_byte
 jsr BS_adr
 jmp @key
@cancel:lda #1
 sta input_cancel
@done:ldy input_len
 lda #$9b
 sta input_buf,y
 jsr CR_adr
 jmp plus_index
.endproc
.proc plus_message
 jsr print_str
 lda #<press_key_msg
 ldx #>press_key_msg
 jsr print_str
 jmp plus_key
.endproc
press_key_msg: .byte 13,10,"Press a key.",$9b
directory_path: .byte "D:VBXE.DIR",$9b

.include "plus_tools.inc"
.include "plus_transfer.inc"
.proc plus_select_idle
 lda CONSOL
 and #1
 bne :+
 jsr plus_tools
: jmp plus_select_actions
.endproc
.proc plus_select_actions
 lda plus_quit
 beq @profile
 cmp #2
 beq @quit
 lda #0
 sta plus_quit
 lda device_type
 beq @close_r
 jsr nclose_n_device
 jmp @closed
@close_r: jsr font_swap_prep_r
@closed: lda #0
 sta dev_ready
 pla
 pla
 jmp device_select
@quit: pla
 pla
 jmp exit_to_dos
@profile:
 lda profile_ready
 jeq @ret
 lda #0
 sta profile_ready
 lda #1
 sta device_type
 sta n_unit
 jsr uninstall_kbd_irq_for_select
 lda profile_rec+481
 and #1
 sta utf8_mode
 lda profile_rec+482
 and #1
 sta atascii_mode
 lda profile_rec+483
 cmp #39
 beq :+
 lda #79
: sta term_last_col
 lda atascii_mode
 beq @ansi_font
 jsr _vbxe_font_from_rom
 jsr plus_font_changed
 jmp @font_ready
@ansi_font: lda profile_rec+484
 cmp #MENU_ITEM_FONT_BASE
 bcc @default_font
 cmp #MENU_ITEM_FONT_BASE+13
 bcc @load_font
@default_font: lda #MENU_ITEM_ATARI
@load_font: sta active_font
 jsr font_load_index
@font_ready: jsr menu_init_labels
 lda profile_rec+32
 cmp #'R'
 bne @network_profile
 lda profile_rec+485
 ldx #5
@baud_check:
 cmp serial_rates,x
 beq @baud_ok
 dex
 bpl @baud_check
 bmi :+
@baud_ok:
 sta serial_baud_code
: pla
 pla
 jmp choose_r
@network_profile:
 lda proto_byte
 cmp #'S'
 bne @connect
 lda #<pw_prompt
 ldx #>pw_prompt
 jsr print_str
 jsr open_k_iocb2
 lda #63
 sta input_limit
 lda #<password_buf
 sta src_ptr
 lda #>password_buf
 sta src_ptr+1
 jsr read_line_vbxe_pw
 jsr nlogin_n_device
@connect:pla
 pla
 jmp skip_nlogin
@ret:rts
.endproc
serial_rates: .byte 8,10,12,13,14,15
