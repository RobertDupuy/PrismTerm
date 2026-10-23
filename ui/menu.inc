; Procomm-inspired local menus. All drawing uses the existing 8x8 cell grid.
ui_str=$d4
ui_cell=$d6
ui_ptr=$d8
ui_list=$da
UI_NORMAL=$cf
UI_HIGHLIGHT=$e0
UI_BORDER=$ce
UI_HINT=$c7
UI_ACCENT=$cb
ui_limit: .byte 80
ui_left: .byte 2
ui_right: .byte 77
ui_top: .byte 3
ui_bottom: .byte 20
ui_tmp: .byte 0
ui_tmp2: .byte 0
ui_loop_i: .byte 0
ui_focus: .byte 0
ui_notice_ptr: .word 0
ui_field_table: .word 0
ui_baud_codes: .byte 8,10,12,13,14,15
ui_baud_lo: .byte <baud300,<baud1200,<baud2400,<baud4800,<baud9600,<baud19200
ui_baud_hi: .byte >baud300,>baud1200,>baud2400,>baud4800,>baud9600,>baud19200
baud300: .byte "300",0
baud1200: .byte "1200",0
baud2400: .byte "2400",0
baud4800: .byte "4800",0
baud9600: .byte "9600",0
baud19200: .byte "19200",0
ui_rows_lo: .repeat 24,i
 .byte <($a100+i*160)
 .endrepeat
ui_rows_hi: .repeat 24,i
 .byte >($a100+i*160)
 .endrepeat

.macro ui_at yy,xx,cc
 lda #yy
 sta ui_y
 lda #xx
 sta ui_x
 lda #cc
 sta ui_color
.endmacro
.macro ui_print label
 lda #<(label)
 ldx #>(label)
 jsr ui_text
.endmacro
.macro line yy,xx,cc,text
 .byte yy,xx,cc,text,0
.endmacro

.proc ui_put
 pha
 lda ui_y
 cmp #24
 bcs @skip
 ldx ui_x
 cpx ui_width
 bcs @skip
 cpx ui_limit
 bcs @skip
 ldx ui_y
 lda ui_rows_lo,x
 sta ui_cell
 lda ui_rows_hi,x
 sta ui_cell+1
 lda ui_x
 asl
 tay
 pla
 sta (ui_cell),y
 iny
 lda ui_color
 sta (ui_cell),y
 inc ui_x
 rts
@skip:pla
 rts
.endproc
.proc ui_text
 sta ui_str
 stx ui_str+1
@next:
 ldy #0
 lda (ui_str),y
 beq @out
 cmp #$9b
 beq @out
 jsr ui_put
 inc ui_str
 bne @next
 inc ui_str+1
 bne @next
@out:rts
.endproc
.proc ui_paint
 sta ui_list
 stx ui_list+1
@line:
 ldy #0
 lda (ui_list),y
 cmp #255
 beq @out
 sta ui_y
 iny
 lda (ui_list),y
 sta ui_x
 iny
 lda (ui_list),y
 sta ui_color
 lda ui_list
 clc
 adc #3
 ldx ui_list+1
 bcc :+
 inx
: jsr ui_text
 lda ui_str
 clc
 adc #1
 sta ui_list
 lda ui_str+1
 adc #0
 sta ui_list+1
 jmp @line
@out:rts
.endproc
.proc ui_fill_row
 lda #0
 sta ui_x
@cell: lda #' '
 jsr ui_put
 lda ui_x
 cmp ui_width
 bcc @cell
 rts
.endproc
.proc ui_clear
 lda term_last_col
 clc
 adc #1
 sta ui_width
 sta ui_limit
 lda #0
 sta ui_y
 lda #$87
 sta ui_color
@row:jsr ui_fill_row
 inc ui_y
 lda ui_y
 cmp #24
 bcc @row
 rts
.endproc
.proc ui_box
 lda #2
 sta ui_left
 lda ui_width
 sec
 sbc #3
 sta ui_right
 lda #20
 sta ui_bottom
 lda ui_page
 bne @geometry
 lda ui_category
 cmp #3
 beq @geometry
 tax
 lda ui_root_counts,x
 asl
 clc
 adc #5
 sta ui_bottom
 lda ui_width
 cmp #40
 beq @geometry
 lda ui_menu_left,x
 sta ui_left
 clc
 adc #34
 sta ui_right
@geometry:
 lda #3
 sta ui_y
@row:
 lda ui_left
 sta ui_x
 lda #UI_NORMAL
 sta ui_color
@space:lda #' '
 jsr ui_put
 lda ui_x
 cmp ui_right
 bcc @space
 beq @space
 lda #UI_BORDER
 sta ui_color
 lda ui_left
 sta ui_x
 lda #179
 jsr ui_put
 lda ui_right
 sta ui_x
 lda #179
 jsr ui_put
 lda ui_y
 cmp ui_bottom
 beq @borders
 inc ui_y
 jmp @row
@borders:
 lda #3
 sta ui_y
 jsr ui_rule
 lda ui_bottom
 sta ui_y
 jsr ui_rule
 lda #3
 sta ui_y
 lda ui_left
 clc
 adc #2
 sta ui_x
 lda #UI_NORMAL
 sta ui_color
 rts
.endproc
ui_menu_left: .byte 2,15,28,2,43
.proc ui_full_box
 lda ui_page
 pha
 lda #6
 sta ui_page
 jsr ui_box
 pla
 sta ui_page
 rts
.endproc
.proc ui_rule
 lda ui_left
 sta ui_x
 lda #UI_BORDER
 sta ui_color
 lda #195
 ldx ui_y
 cpx #3
 bne :+
 lda #218
: cpx ui_bottom
 bne :+
 lda #192
: jsr ui_put
@h:lda #196
 jsr ui_put
 lda ui_x
 cmp ui_right
 bcc @h
 lda #180
 ldx ui_y
 cpx #3
 bne :+
 lda #191
: cpx ui_bottom
 bne :+
 lda #217
: jmp ui_put
.endproc
.proc ui_palette
 lda #2
 sta psel
 lda #0
 sta ui_tmp
@entry:
 lda ui_tmp
 sta csel
 bmi @background
 and #15
 jmp @index
@background:lsr
 lsr
 lsr
 lsr
 and #7
@index:sta ui_tmp2
 asl
 clc
 adc ui_tmp2
 tax
 lda ui_colors,x
 sta cr
 lda ui_colors+1,x
 sta cg
 lda ui_colors+2,x
 sta cb
 inc ui_tmp
 bne @entry
 lda memac_bank_sel
 pha
 lda #$80
 sta memac_bank_sel
 lda #$21
 sta $a803
 pla
 sta memac_bank_sel
 rts
.endproc
ui_colors:
 .byte 8,15,29,180,75,97,71,136,101,255,223,131
 .byte 19,44,96,136,81,154,135,230,238,195,206,222
 .byte 86,100,126,241,123,130,155,224,176,255,223,131
 .byte 128,168,236,218,160,232,135,230,238,244,247,255

.proc ui_shell
 lda #0
 sta ui_y
 lda #$cf
 sta ui_color
 jsr ui_fill_row
 ui_at 0,2,$cf
 ui_print ui_brand
 lda ui_width
 sec
 sbc #13
 sta ui_x
 lda #$b0
 sta ui_color
 ui_print ui_local
 lda #1
 sta ui_y
 lda #UI_NORMAL
 sta ui_color
 jsr ui_fill_row
 lda #0
 sta ui_loop_i
@tabs:
 ldx ui_loop_i
 lda ui_width
 cmp #40
 beq @short
 lda ui_tab_x,x
 sta ui_x
 lda ui_tabs_lo,x
 pha
 lda ui_tabs_hi,x
 tax
 pla
 jmp @tab
@short:
 lda ui_short_x,x
 sta ui_x
 lda ui_short_lo,x
 pha
 lda ui_short_hi,x
 tax
 pla
@tab:
 pha
 lda #UI_NORMAL
 ldy ui_loop_i
 cpy ui_category
 bne :+
 lda #UI_HIGHLIGHT
: sta ui_color
 pla
 jsr ui_text
 inc ui_loop_i
 lda ui_loop_i
 cmp #5
 bcc @tabs
 lda #22
 sta ui_y
 lda #UI_NORMAL
 sta ui_color
 jsr ui_fill_row
 ui_at 22,1,$ca
 lda dev_ready
 beq @offline
 ui_print ui_online
 jmp @device
@offline:ui_print ui_offline
@device:
 lda #10
 sta ui_x
 lda device_type
 bne @network
 ui_print ui_serial
 jsr ui_baud_text
 jmp @encoding
@network:ui_print ui_fuji
@encoding:
 lda #24
 sta ui_x
 lda dialog_core+(utf8_mode-plus_state_start)
 beq :+
 ui_print ui_utf8
 jmp @capture
: ui_print ui_cp437
@capture:
 lda ui_width
 cmp #40
 beq @footer
 lda #42
 sta ui_x
 lda dialog_atascii
 beq :+
 ui_print ui_atascii
 jmp @cap
: ui_print ui_ansi
@cap:
 lda #56
 sta ui_x
 lda capture_full
 beq :+
 ui_print ui_cap_full
 jmp @footer
:
 lda capture_on
 beq :+
 ui_print ui_cap_on
 jmp @footer
: ui_print ui_cap_off
@footer:
 lda #23
 sta ui_y
 lda #$e0
 sta ui_color
 jsr ui_fill_row
 lda #1
 sta ui_x
 lda ui_width
 cmp #40
 beq :+
 ui_print ui_keys
 rts
: ui_print ui_keys40
 rts
.endproc
.proc ui_baud_text
 ldx #5
@find:lda ui_baud_codes,x
 cmp serial_baud_code
 beq @found
 dex
 bpl @find
 ldx #4
@found:lda ui_baud_lo,x
 pha
 lda ui_baud_hi,x
 tax
 pla
 jmp ui_text
.endproc

.proc ui_loop
 jsr ui_palette
redraw:
 jsr ui_clear
 lda ui_page
 bne :+
 lda ui_category
 cmp #3
 beq :+
 jsr ui_background
 lda #0
 sta ui_y
@dimrow:
 ldx ui_y
 lda ui_rows_lo,x
 sta ui_cell
 lda ui_rows_hi,x
 sta ui_cell+1
 ldy #1
@dimcell:lda #$88
 sta (ui_cell),y
 iny
 iny
 cpy #160
 bcc @dimcell
 inc ui_y
 lda ui_y
 cmp #24
 bcc @dimrow
: jsr ui_shell
 jsr ui_box
 lda ui_page
 cmp #1
 jeq ui_directory_loop
 cmp #2
 jeq ui_transfer_loop
 cmp #3
 jeq ui_profile_loop
 cmp #4
 jeq ui_font_loop
 cmp #5
 jeq ui_baud_loop
 jsr ui_draw_root
@key:
 jsr ui_refresh
 jsr ui_key
 cmp #27
 jeq @exit
 cmp #$83
 beq @left
 cmp #'<'
 beq @left
 cmp #$82
 beq @right
 cmp #'>'
 beq @right
 cmp #9
 beq @right
 cmp #$80
 beq @up
 cmp #'-'
 beq @up
 cmp #$81
 beq @down
 cmp #'='
 beq @down
 cmp #13
 jeq @select
 cmp #'?'
 beq @help
 and #$df
 ldx #0
@letter:
 cmp ui_root_keys,x
 beq @category
 inx
 cpx #5
 bcc @letter
 jmp @key
@category:stx ui_category
 jmp @reset
@help:lda #4
 sta ui_category
 jmp @reset
@left:dec ui_category
 bpl @reset
 lda #4
 sta ui_category
 bne @reset
@right:inc ui_category
 lda ui_category
 cmp #5
 bcc @reset
 lda #0
 sta ui_category
@reset:lda #0
 sta ui_selected
 jmp redraw
@up:lda ui_selected
 jeq @key
 dec ui_selected
 jmp redraw
@down:lda ui_selected
 clc
 adc #1
 cmp ui_count
 jcs @key
 sta ui_selected
 jmp redraw
@select:jsr ui_root_action
 lda ui_action
 bne @exit
 jmp redraw
@exit:rts
.endproc
ui_redraw=ui_loop::redraw

.proc ui_draw_root
 ldx ui_category
 lda ui_tabs_lo,x
 pha
 lda ui_tabs_hi,x
 tax
 pla
 jsr ui_text
 ldx ui_category
 lda ui_root_counts,x
 sta ui_count
 lda ui_root_lo,x
 sta ui_field_table
 lda ui_root_hi,x
 sta ui_field_table+1
 lda #0
 sta ui_index
@row:
 lda ui_index
 asl
 clc
 adc #5
 sta ui_y
 lda ui_left
 clc
 adc #2
 sta ui_x
 lda #UI_NORMAL
 ldx ui_index
 cpx ui_selected
 bne :+
 lda #UI_HIGHLIGHT
: sta ui_color
@fill:lda #' '
 jsr ui_put
 lda ui_x
 cmp ui_right
 bcc @fill
 lda ui_left
 clc
 adc #3
 sta ui_x
 lda ui_field_table
 sta ui_ptr
 lda ui_field_table+1
 sta ui_ptr+1
 lda ui_index
 asl
 tay
 lda (ui_ptr),y
 pha
 iny
 lda (ui_ptr),y
 tax
 pla
 jsr ui_text
 lda ui_category
 cmp #3
 bne @next
 lda #23
 sta ui_x
 jsr ui_display_value
@next:inc ui_index
 lda ui_index
 cmp ui_count
 bcc @row
 lda ui_category
 cmp #3
 bne @out
 lda #17
 sta ui_y
 lda ui_left
 clc
 adc #3
 sta ui_x
 lda #UI_HINT
 sta ui_color
 ui_print ui_preview1
 lda #18
 sta ui_y
 lda ui_left
 clc
 adc #3
 sta ui_x
 ui_print ui_preview2
@out:rts
.endproc
.proc ui_display_value
 lda ui_index
 beq @mode
 cmp #1
 beq @width
 cmp #2
 beq @encoding
 lda active_font
 sec
 sbc #3
 asl
 tax
 lda ui_fonts,x
 pha
 lda ui_fonts+1,x
 tax
 pla
 jmp ui_text
@mode:lda dialog_atascii
 beq :+
 ui_print ui_atascii
 rts
: ui_print ui_ansi
 rts
@width:lda term_last_col
 cmp #39
 beq :+
 ui_print ui_col80
 rts
: ui_print ui_col40
 rts
@encoding:lda dialog_core+(utf8_mode-plus_state_start)
 beq :+
 ui_print ui_utf8
 rts
: ui_print ui_cp437
 rts
.endproc

.proc ui_root_action
 lda ui_category
 beq @connect
 cmp #1
 beq @transfer
 cmp #2
 jeq @session
 cmp #3
 jeq @display
 jmp ui_help
@connect:
 lda ui_selected
 beq @directory
 cmp #1
 beq @new
 cmp #2
 beq @save
 cmp #3
 beq @serial
 cmp #4
 beq @disconnect
 lda #2
 sta ui_arg
 ui_print ui_empty
 jmp ui_confirm_exit
@disconnect:
 lda #1
 sta ui_arg
 jmp ui_confirm_exit
@directory:lda #1
 sta ui_page
 lda #0
 sta ui_selected
 sta ui_profile_mode
 rts
@new:lda #0
 sta ui_profile_mode
 jsr ui_new_profile
 lda #3
 sta ui_page
 rts
@save:lda #1
 sta ui_profile_mode
 lda #1
 sta ui_page
 lda #0
 sta ui_selected
 rts
@serial:lda #5
 sta ui_page
 lda #0
 sta ui_selected
 rts
@transfer:
 lda dev_ready
 bne :+
 lda #<ui_need_connection
 ldx #>ui_need_connection
 jmp ui_notice
: lda ui_selected
 sta xdirection
 lda #0
 sta ui_selected
 sta ui_resume
 lda #2
 sta ui_protocol
 sta ui_page
 lda #$9b
 sta io_name
 rts
@session:
 lda ui_selected
 bne :+
 lda #1
 sta ui_action
 rts
: cmp #1
 bne :+
 lda capture_on
 eor #1
 sta capture_on
 rts
: cmp #2
 beq @savecap
 cmp #3
 beq @macro
 cmp #4
 bne @status
 lda #0
 sta macro_pos
 sta macro_delay
 lda #1
 sta macro_running
 sta ui_action
 inc ui_action
 rts
@status:jmp ui_status
@savecap:jmp ui_capture
@macro:jmp ui_macro
@display:
 lda ui_selected
 cmp #2
 beq @encoding
 cmp #3
 beq @fonts
 clc
 adc #2
 sta ui_cmd
 jmp ui_command
@encoding:
 lda dialog_core+(utf8_mode-plus_state_start)
 eor #1
 sta dialog_core+(utf8_mode-plus_state_start)
 lda #0
 sta dialog_core+(utf_left-plus_state_start)
 rts
@fonts:lda #4
 sta ui_page
 lda active_font
 sec
 sbc #3
 sta ui_selected
 rts
.endproc

ui_brand: .byte "PRISMTERM 0.3",0
ui_local: .byte " LOCAL MENU ",0
ui_online: .byte "ONLINE",0
ui_offline: .byte "OFFLINE",0
ui_serial: .byte "R: ",0
ui_fuji: .byte "N: FUJINET",0
ui_utf8: .byte "UTF-8",0
ui_cp437: .byte "CP437",0
ui_ansi: .byte "ANSI",0
ui_atascii: .byte "ATASCII",0
ui_col80: .byte "80 columns",0
ui_col40: .byte "40 columns",0
ui_cap_on: .byte "CAPTURE ON",0
ui_cap_off: .byte "CAPTURE OFF",0
ui_cap_full: .byte "CAPTURE FULL",0
ui_keys: .byte 27,26," Menu  ",24,25," Item  RETURN Open  ESC To BBS  ? Help",0
ui_keys40: .byte "</> Menu  -/= Item  RETURN  ESC Back",0
ui_root_keys: .byte "CTSDH"
ui_tab_x: .byte 2,15,29,42,55
ui_short_x: .byte 1,8,15,22,29
ui_tabs_lo: .byte <ui_connect,<ui_transfer,<ui_session,<ui_display,<ui_help_name
ui_tabs_hi: .byte >ui_connect,>ui_transfer,>ui_session,>ui_display,>ui_help_name
ui_short_lo: .byte <ui_conn,<ui_xfer,<ui_sess,<ui_disp,<ui_help_name
ui_short_hi: .byte >ui_conn,>ui_xfer,>ui_sess,>ui_disp,>ui_help_name
ui_connect: .byte " Connect ",0
ui_transfer: .byte " Transfer ",0
ui_session: .byte " Session ",0
ui_display: .byte " Display ",0
ui_help_name: .byte " Help ",0
ui_conn: .byte "Conn",0
ui_xfer: .byte "Xfer",0
ui_sess: .byte "Sess",0
ui_disp: .byte "Disp",0
ui_root_counts: .byte 6,2,6,4,3
ui_root_lo: .byte <ui_connect_items,<ui_transfer_items,<ui_session_items,<ui_display_items,<ui_help_items
ui_root_hi: .byte >ui_connect_items,>ui_transfer_items,>ui_session_items,>ui_display_items,>ui_help_items
ui_connect_items: .word ui_dir_name,ui_quick_name,ui_save_name,ui_serial_name,ui_disconnect_name,ui_exit_name
ui_transfer_items: .word ui_receive_name,ui_send_name
ui_session_items: .word ui_history_name,ui_capture_name,ui_savecap_name,ui_macro_name,ui_runmacro_name,ui_status_name
ui_display_items: .word ui_mode_name,ui_width_name,ui_encoding_name,ui_font_name
ui_help_items: .word ui_keyboard_name,ui_transfer_help,ui_about_name
ui_dir_name: .byte "Connection directory...",0
ui_quick_name: .byte "Quick connect / new profile...",0
ui_save_name: .byte "Save current connection...",0
ui_serial_name: .byte "Serial port / baud rate...",0
ui_disconnect_name: .byte "Disconnect...",0
ui_exit_name: .byte "Exit to DOS...",0
ui_receive_name: .byte "Receive a file...",0
ui_send_name: .byte "Send a file...",0
ui_history_name: .byte "Browse scrollback",0
ui_capture_name: .byte "Start / stop capture",0
ui_savecap_name: .byte "Save capture...",0
ui_macro_name: .byte "Edit login macro...",0
ui_runmacro_name: .byte "Run login macro",0
ui_status_name: .byte "Session status",0
ui_mode_name: .byte "Terminal",0
ui_width_name: .byte "Columns",0
ui_encoding_name: .byte "Encoding",0
ui_font_name: .byte "Font...",0
ui_keyboard_name: .byte "Keyboard guide",0
ui_transfer_help: .byte "Transfers and Unicode",0
ui_about_name: .byte "About / credits",0
ui_preview1: .byte "Aa Bb Cc 0123456789",0
ui_preview2: .byte 218,196,196,196,191,"  ",219,178,177,176,"  caf",130," ",156," ",227,0
ui_empty: .byte 0
ui_need_connection: .byte "Connect before transferring.",0

.include "ui/forms.inc"
