; Shared form editor: edits a copy, commits only on RETURN, ESC leaves original.
.proc ui_edit
 lda ui_edit_ptr
 sta ui_ptr
 lda ui_edit_ptr+1
 sta ui_ptr+1
 ldy #0
@copy:lda (ui_ptr),y
 beq @end
 cmp #$9b
 beq @end
 cpy ui_edit_max
 bcs @end
 sta input_buf,y
 iny
 bne @copy
@end:sty ui_edit_len
 lda #$9b
 sta input_buf,y
 lda #0
 sta input_cancel
@draw:
 lda ui_edit_y
 sta ui_y
 lda #5
 sta ui_x
 lda #$8f
 sta ui_color
 lda ui_width
 sec
 sbc #6
 sta ui_limit
@blank:lda #' '
 jsr ui_put
 lda ui_x
 cmp ui_limit
 bcc @blank
 lda ui_edit_len
 sec
 sbc ui_width
 clc
 adc #12
 bpl :+
 lda #0
: sta ui_edit_offset
 lda #5
 sta ui_x
 ldx ui_edit_offset
@text:cpx ui_edit_len
 beq @cursor
 lda input_buf,x
 ldy ui_edit_mask
 beq :+
 lda #'*'
: stx ui_tmp
 jsr ui_put
 ldx ui_tmp
 inx
 jmp @text
@cursor:lda #'_'
 jsr ui_put
 jsr ui_refresh
 jsr ui_key
 cmp #27
 beq @cancel
 cmp #13
 beq @commit
 cmp #8
 beq @back
 cmp #127
 beq @back
 cmp #126
 beq @back
 cmp #32
 bcc @draw
 cmp #127
 jcs @draw
 ldy ui_edit_len
 cpy ui_edit_max
 jcs @draw
 sta input_buf,y
 inc ui_edit_len
 jmp @draw
@back:lda ui_edit_len
 jeq @draw
 dec ui_edit_len
 jmp @draw
@cancel:lda #1
 sta input_cancel
 bne @done
@commit:
 ldy ui_edit_len
 lda #$9b
 sta input_buf,y
 lda ui_edit_ptr
 sta ui_ptr
 lda ui_edit_ptr+1
 sta ui_ptr+1
@store:lda input_buf,y
 sta (ui_ptr),y
 dey
 bpl @store
@done:lda ui_width
 sta ui_limit
 rts
.endproc
.proc ui_notice
 sta ui_notice_ptr
 stx ui_notice_ptr+1
 jsr ui_clear
 jsr ui_shell
 jsr ui_full_box
 ui_print ui_notice_title
 ui_at 7,5,UI_NORMAL
 lda ui_notice_ptr
 ldx ui_notice_ptr+1
 jsr ui_text
 ui_at 17,5,UI_ACCENT
 ui_print ui_press_key
 jsr ui_refresh
 jmp ui_key
.endproc
.proc ui_confirm_exit
 lda #<ui_disconnect_question
 ldx #>ui_disconnect_question
 ldy ui_arg
 cpy #2
 bne :+
 lda #<ui_exit_question
 ldx #>ui_exit_question
: jsr ui_notice
 and #$df
 cmp #'Y'
 bne @out
 lda ui_arg
 sta plus_quit
 lda #2
 sta ui_action
@out:rts
.endproc
ui_notice_title: .byte " NOTICE ",0
ui_press_key: .byte "Press a key to continue.",0
ui_disconnect_question: .byte "Disconnect?  Y = yes, ESC = no",0
ui_exit_question:
.ifdef PRISM_CART
 .byte "Restart? Y = yes, ESC = no",0
.else
 .byte "Exit to DOS? Y = yes, ESC = no",0
.endif

.proc ui_help
 lda ui_selected
 sta ui_focus
 jsr ui_clear
 jsr ui_shell
 jsr ui_full_box
 ui_print ui_help_name
 lda ui_focus
 beq @keys
 cmp #1
 beq @transfer
 lda #<ui_about_lines
 ldx #>ui_about_lines
 jmp @paint
@keys:lda #<ui_help_lines
 ldx #>ui_help_lines
 jmp @paint
@transfer:lda #<ui_transfer_lines
 ldx #>ui_transfer_lines
@paint:jsr ui_paint
 jsr ui_refresh
 jmp ui_key
.endproc
ui_help_lines:
 line 5,4,UI_NORMAL,"START opens the command menu."
 line 7,4,UI_NORMAL,"OPTION opens Display directly."
 line 9,4,UI_NORMAL,"Left/right or < >: menu category"
 line 11,4,UI_NORMAL,"Up/down or - =: select an item"
 line 13,4,UI_NORMAL,"RETURN opens; ESC goes back."
 line 15,4,UI_NORMAL,"Type C/T/S/D/H for a category."
 line 17,4,UI_HINT,"Outside local menus, keys go"
 line 18,4,UI_HINT,"to the BBS. All 24 rows return."
 .byte 255
ui_transfer_lines:
 line 5,4,UI_NORMAL,"Start the transfer on the BBS,"
 line 6,4,UI_NORMAL,"then choose Send or Receive."
 line 8,4,UI_NORMAL,"Binary Telnet is required."
 line 9,4,UI_NORMAL,"Raw TCP and R: also work."
 line 11,4,UI_NORMAL,"ESC cancels; partial files stay."
 line 13,4,UI_NORMAL,"UTF-8: native Unicode glyphs."
 line 15,4,UI_NORMAL,"Complex layout uses the host"
 line 16,4,UI_NORMAL,"companion. Exit it to transfer."
 line 18,4,UI_HINT,"Press a key to return."
 .byte 255
ui_about_lines:
 line 5,4,UI_BORDER,"PrismTerm 0.3"
 line 7,4,UI_NORMAL,"Your Atari. The whole world."
 line 9,4,UI_NORMAL,"Maintainer: Robert Dupuy"
 line 11,4,UI_NORMAL,"Based on Brad Colbert's work"
 line 12,4,UI_NORMAL,"and Joseph Zatarski's terminal."
 line 14,4,UI_NORMAL,"Extensions: OpenAI Codex"
 line 16,4,UI_HINT,"Menu inspiration: DOS Procomm."
 line 18,4,UI_HINT,"Press a key to return."
 .byte 255

.proc ui_font_loop
 ui_print ui_font_name
 lda #0
 sta ui_index
@draw:lda ui_index
 clc
 adc #5
 sta ui_y
 lda #5
 sta ui_x
 lda #UI_NORMAL
 ldx ui_index
 cpx ui_selected
 bne :+
 lda #UI_HIGHLIGHT
: sta ui_color
 lda ui_index
 asl
 tax
 lda ui_fonts,x
 pha
 lda ui_fonts+1,x
 tax
 pla
 jsr ui_text
 inc ui_index
 lda ui_index
 cmp #13
 bcc @draw
 ui_at 19,5,UI_HINT
 ui_print ui_preview1
@key:jsr ui_refresh
 jsr ui_key
 cmp #27
 beq ui_form_back
 cmp #13
 beq @select
 cmp #$80
 beq @up
 cmp #'-'
 beq @up
 cmp #$81
 beq @down
 cmp #'='
 bne @key
@down:lda ui_selected
 cmp #12
 bcs @key
 inc ui_selected
 jmp ui_redraw
@up:lda ui_selected
 beq @key
 dec ui_selected
 jmp ui_redraw
@select:lda #1
 sta ui_cmd
 lda ui_selected
 clc
 adc #3
 sta ui_arg
 jsr ui_command
 jmp ui_form_back
.endproc
ui_form_back:
 lda #0
 sta ui_page
 sta ui_selected
 jmp ui_redraw
ui_fonts: .word uf_ibm,uf_atari,uf_asc,uf_balloon,uf_bozo,uf_bzzz,uf_casual,uf_computer,uf_cursive,uf_hero,uf_news,uf_preppie,uf_shadow
uf_ibm: .byte "IBMPC",0
uf_atari: .byte "AtariPC",0
uf_asc: .byte "AscPrint",0
uf_balloon: .byte "Balloon",0
uf_bozo: .byte "Bozo",0
uf_bzzz: .byte "Bzzz2",0
uf_casual: .byte "Casual GT",0
uf_computer: .byte "Computer",0
uf_cursive: .byte "Cursive",0
uf_hero: .byte "Hero",0
uf_news: .byte "Newsletter",0
uf_preppie: .byte "Preppie",0
uf_shadow: .byte "Shadow",0

.proc ui_baud_loop
 ui_print ui_serial_title
 ui_at 5,5,UI_HINT
 ui_print ui_baud_help
 lda #0
 sta ui_index
@row:lda ui_index
 clc
 adc #7
 sta ui_y
 lda #6
 sta ui_x
 lda #UI_NORMAL
 ldx ui_index
 cpx ui_selected
 bne :+
 lda #UI_HIGHLIGHT
: sta ui_color
 lda ui_baud_lo,x
 pha
 lda ui_baud_hi,x
 tax
 pla
 jsr ui_text
 inc ui_index
 lda ui_index
 cmp #6
 bcc @row
 ui_at 15,5,UI_NORMAL
 ui_print ui_current_baud
 jsr ui_baud_text
 ui_at 17,5,UI_ACCENT
 ui_print ui_baud_connect
@key:jsr ui_refresh
 jsr ui_key
 cmp #27
 jeq ui_form_back
 cmp #13
 beq @apply
 cmp #'c'
 beq @connect
 cmp #'C'
 beq @connect
 cmp #$80
 beq @up
 cmp #'-'
 beq @up
 cmp #$81
 beq @down
 cmp #'='
 bne @key
@down:lda ui_selected
 cmp #5
 bcs @key
 inc ui_selected
 jmp ui_redraw
@up:lda ui_selected
 beq @key
 dec ui_selected
 jmp ui_redraw
@apply:lda #4
 sta ui_cmd
 ldx ui_selected
 lda ui_baud_codes,x
 sta ui_arg
 jsr ui_command
 jmp ui_redraw
@connect:
 lda dev_ready
 bne @key
 lda #4
 sta ui_cmd
 ldx ui_selected
 lda ui_baud_codes,x
 sta ui_arg
 jsr ui_command
 jsr ui_new_profile
 lda #'R'
 sta profile_rec+32
 lda #':'
 sta profile_rec+33
 lda #$9b
 sta profile_rec+34
 jsr ui_connect_record
 rts
.endproc
ui_baud_help: .byte "Select baud, then press RETURN.",0
ui_serial_title: .byte " SERIAL PORT ",0
ui_current_baud: .byte "Current: ",0
ui_baud_connect: .byte "C Connect serial / ESC Back",0

; Decimal 32-bit output, using a private copy rather than protocol counters.
.proc ui_decimal
 lda #0
 sta ui_tmp2
@divide:lda #0
 ldx #32
@bit:asl ui_number
 rol ui_number+1
 rol ui_number+2
 rol ui_number+3
 rol
 cmp #10
 bcc :+
 sbc #10
 inc ui_number
: dex
 bne @bit
 ldx ui_tmp2
 clc
 adc #'0'
 sta ui_digits,x
 inc ui_tmp2
 lda ui_number
 ora ui_number+1
 ora ui_number+2
 ora ui_number+3
 bne @divide
@print:dec ui_tmp2
 ldx ui_tmp2
 lda ui_digits,x
 jsr ui_put
 lda ui_tmp2
 bne @print
 rts
.endproc
.proc ui_number16
 sta ui_number
 stx ui_number+1
 lda #0
 sta ui_number+2
 sta ui_number+3
 jmp ui_decimal
.endproc
.proc ui_status
 jsr ui_clear
 jsr ui_shell
 jsr ui_full_box
 ui_print ui_status_name
 ui_at 6,5,UI_NORMAL
 ui_print ui_history_count
 lda history_count
 ldx history_count+1
 jsr ui_number16
 ui_at 8,5,UI_NORMAL
 ui_print ui_capture_count
 lda capture_size
 ldx capture_size+1
 jsr ui_number16
 ui_at 10,5,UI_NORMAL
 ui_print ui_encoding_name
 lda #23
 sta ui_x
 lda #2
 sta ui_index
 jsr ui_display_value
 ui_at 13,5,UI_HINT
 ui_print ui_unicode_note1
 ui_at 14,5,UI_HINT
 ui_print ui_unicode_note2
 ui_at 18,5,UI_ACCENT
 ui_print ui_press_key
 jsr ui_refresh
 jmp ui_key
.endproc
ui_history_count: .byte "Scrollback lines: ",0
ui_capture_count: .byte "Capture bytes: ",0
ui_unicode_note1: .byte "UTF-8: native Unicode glyphs.",0
ui_unicode_note2: .byte "Complex layout: host companion.",0
.proc ui_macro
 jsr ui_clear
 jsr ui_shell
 jsr ui_full_box
 ui_print ui_macro_name
 ui_at 7,5,UI_HINT
 ui_print ui_macro_help
 ui_at 15,5,UI_HINT
 ui_print ui_edit_help
 lda #<login_macro
 sta ui_edit_ptr
 lda #>login_macro
 sta ui_edit_ptr+1
 lda #127
 sta ui_edit_max
 lda #10
 sta ui_edit_y
 lda #0
 sta ui_edit_mask
 jmp ui_edit
.endproc
ui_macro_help: .byte "^M = RETURN    ~ = 1 sec pause",0
ui_edit_help: .byte "RETURN saves / ESC cancels",0

.proc ui_filename
 ; Title A/X; return input_cancel and io_name.
 sta ui_notice_ptr
 stx ui_notice_ptr+1
 jsr ui_clear
 jsr ui_shell
 jsr ui_full_box
 lda ui_notice_ptr
 ldx ui_notice_ptr+1
 jsr ui_text
 ui_at 7,5,UI_NORMAL
 ui_print ui_local_file
 ui_at 13,5,UI_HINT
 ui_print ui_filename_help
 lda #<io_name
 sta ui_edit_ptr
 lda #>io_name
 sta ui_edit_ptr+1
 lda #63
 sta ui_edit_max
 lda #10
 sta ui_edit_y
 lda #0
 sta ui_edit_mask
 jmp ui_edit
.endproc
ui_local_file: .byte "Local Atari filename:",0
ui_filename_help: .byte "Example: D:FILE.BIN",0
.proc ui_capture
 lda #$9b
 sta io_name
 lda #<ui_savecap_name
 ldx #>ui_savecap_name
 jsr ui_filename
 lda input_cancel
 jne @out
 lda io_name
 cmp #$9b
 jeq @out
 lda #8
 sta io_mode
 jsr ui_file_open
 lda io_error
 bne @error
 lda #0
 sta io_page
@page:
 lda io_page
 cmp capture_size+1
 bcc @full
 bne @finish
 lda capture_size
 beq @finish
 sta io_size
 lda #0
 sta io_size+1
 jmp @write
@full:lda #0
 sta io_size
 lda #1
 sta io_size+1
@write:
 lda memac_bank_sel
 pha
 lda io_page
 lsr
 lsr
 lsr
 lsr
 ora #$f0
 sta memac_bank_sel
 lda io_page
 and #15
 ora #$a0
 sta io_ptr+1
 lda #0
 sta io_ptr
 jsr ui_file_write
 pla
 sta memac_bank_sel
 lda io_error
 bne @close_error
 inc io_page
 jmp @page
@finish:jsr ui_file_close
 lda io_error
 bne @error
 lda #0
 sta capture_size
 sta capture_size+1
 sta capture_full
 lda #<ui_saved
 ldx #>ui_saved
 jmp ui_notice
@close_error:jsr ui_file_close
@error:lda #<ui_disk_error
 ldx #>ui_disk_error
 jmp ui_notice
@out:rts
.endproc
ui_saved: .byte "Saved.",0
ui_disk_error: .byte "Disk error. Data was retained.",0

.include "ui/profiles.inc"
.include "ui/transfers.inc"
