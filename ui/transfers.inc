.proc ui_transfer_loop
 lda xdirection
 beq :+
 ui_print ui_send_name
 jmp @body
: ui_print ui_receive_name
@body:
 ui_at 5,5,UI_HINT
 lda xdirection
 beq :+
 ui_print ui_to_bbs
 jmp @fields
: ui_print ui_to_atari
@fields:
 lda #0
 sta ui_index
@row:
 lda ui_index
 asl
 clc
 adc #7
 sta ui_y
 lda #4
 sta ui_x
 lda #UI_NORMAL
 ldx ui_index
 cpx ui_selected
 bne :+
 lda #UI_HIGHLIGHT
: sta ui_color
 lda ui_xfer_labels_lo,x
 pha
 lda ui_xfer_labels_hi,x
 tax
 pla
 jsr ui_text
 lda #18
 sta ui_x
 lda ui_right
 sta ui_limit
 lda ui_index
 beq @protocol
 cmp #1
 beq @filename
 cmp #2
 beq @resume
 jmp @next
@protocol:jsr ui_xfer_protocol_text
 jmp @next
@filename:ui_print io_name
 jmp @next
@resume:
 lda ui_protocol
 cmp #2
 bne @na
 lda xdirection
 bne @na
 lda ui_resume
 beq :+
 ui_print ui_yes
 jmp @next
: ui_print ui_no
 jmp @next
@na:ui_print ui_na
@next:lda ui_width
 sta ui_limit
 inc ui_index
 lda ui_index
 cmp #4
 jcc @row
 ui_at 16,4,UI_HINT
 ui_print ui_start_remote
 ui_at 18,4,UI_HINT
 ui_print ui_transfer_keys
@key:
 jsr ui_refresh
 jsr ui_key
 cmp #27
 jeq ui_form_back
 cmp #13
 beq @select
 cmp #$82
 beq @right
 cmp #$83
 beq @left
 cmp #$80
 beq @up
 cmp #'-'
 beq @up
 cmp #$81
 beq @down
 cmp #'='
 bne @key
@down:lda ui_selected
 cmp #3
 bcs @key
 inc ui_selected
 jmp ui_redraw
@up:lda ui_selected
 beq @key
 dec ui_selected
 jmp ui_redraw
@left:lda ui_selected
 bne @key
 dec ui_protocol
 lda ui_protocol
 and #3
 sta ui_protocol
 jmp ui_redraw
@right:lda ui_selected
 bne @key
 jmp @next_protocol
@select:lda ui_selected
 beq @next_protocol
 cmp #1
 beq @editfile
 cmp #2
 beq @toggle_resume
 jsr ui_begin_transfer
 jmp ui_form_back
@next_protocol:inc ui_protocol
 lda ui_protocol
 and #3
 sta ui_protocol
 jmp ui_redraw
@editfile:
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
 jsr ui_edit
 jmp ui_redraw
@toggle_resume:
 lda ui_protocol
 cmp #2
 jne @key
 lda xdirection
 jne @key
 lda ui_resume
 eor #1
 sta ui_resume
 jmp ui_redraw
.endproc
.proc ui_xfer_protocol_text
 lda ui_protocol
 asl
 tax
 lda ui_protocol_names,x
 pha
 lda ui_protocol_names+1,x
 tax
 pla
 jmp ui_text
.endproc
.proc ui_begin_transfer
 lda io_name
 cmp #$9b
 beq @filename
 cmp #0
 beq @filename
 lda ui_protocol
 sta xmode
 lda xdirection
 bne @start
 lda ui_resume
 beq @check_existing
 lda xmode
 cmp #2
 bne @check_existing
 lda #4
 sta xmode
 bne @start
@check_existing:
 lda #4
 sta io_mode
 jsr ui_file_open
 lda io_error
 cmp #170
 beq @start
 cmp #0
 bne @disk_error
 jsr ui_file_close
 lda #<ui_replace_file
 ldx #>ui_replace_file
 jsr ui_notice
 and #$df
 cmp #'Y'
 bne @out
@start:
 lda #0
 ldx #3
@zero:sta ui_rx_bytes,x
 sta ui_tx_bytes,x
 dex
 bpl @zero
 lda #1
 sta ui_xfer_active
 jsr ui_progress_draw
 lda #5
 sta ui_cmd
 jsr ui_command
 lda #0
 sta ui_xfer_active
 lda xresult
 cmp #8
 bcc :+
 lda #1
: asl
 tax
 lda ui_results,x
 pha
 lda ui_results+1,x
 tax
 pla
 jmp ui_notice
@filename:lda #<ui_need_filename
 ldx #>ui_need_filename
 jmp ui_notice
@disk_error:lda #<ui_disk_error
 ldx #>ui_disk_error
 jmp ui_notice
@out:rts
.endproc
.proc ui_progress_draw
 jsr ui_clear
 jsr ui_shell
 jsr ui_box
 lda xdirection
 beq :+
 ui_print ui_sending
 jmp @body
: ui_print ui_receiving
@body:
 ui_at 6,5,UI_BORDER
 jsr ui_xfer_protocol_text
 ui_at 8,5,UI_NORMAL
 ui_print io_name
 ui_at 11,5,UI_NORMAL
 ui_print ui_rx_label
 ldx #3
@rx:lda ui_rx_bytes,x
 sta ui_number,x
 dex
 bpl @rx
 jsr ui_decimal
 ui_at 13,5,UI_NORMAL
 ui_print ui_tx_label
 ldx #3
@tx:lda ui_tx_bytes,x
 sta ui_number,x
 dex
 bpl @tx
 jsr ui_decimal
 ui_at 16,5,UI_HINT
 ui_print ui_wire_note
 ui_at 18,5,UI_ACCENT
 ui_print ui_cancel_transfer
 jmp ui_refresh
.endproc
ui_xfer_labels_lo: .byte <ui_proto_label,<ui_file_label,<ui_resume_label,<ui_begin_label
ui_xfer_labels_hi: .byte >ui_proto_label,>ui_file_label,>ui_resume_label,>ui_begin_label
ui_protocol_names: .word ui_xmodem_label,ui_ymodem_label,ui_zmodem_label,ui_kermit_label
ui_xmodem_label: .byte "XMODEM-CRC",0
ui_ymodem_label: .byte "YMODEM",0
ui_zmodem_label: .byte "ZMODEM",0
ui_kermit_label: .byte "Kermit",0
ui_to_bbs: .byte "Direction: Atari -> BBS",0
ui_to_atari: .byte "Direction: BBS -> Atari",0
ui_file_label: .byte "Local file",0
ui_resume_label: .byte "Resume",0
ui_begin_label: .byte "Begin transfer",0
ui_yes: .byte "Yes",0
ui_no: .byte "No",0
ui_na: .byte "ZMODEM receive only",0
ui_start_remote: .byte "Start the matching BBS transfer.",0
ui_transfer_keys: .byte "RETURN selects / ESC goes back",0
ui_need_filename: .byte "Enter a local filename first.",0
ui_replace_file: .byte "Replace file? Y=yes ESC=no",0
ui_sending: .byte " SENDING FILE ",0
ui_receiving: .byte " RECEIVING FILE ",0
ui_rx_label: .byte "Wire RX bytes: ",0
ui_tx_label: .byte "Wire TX bytes: ",0
ui_wire_note: .byte "Counts include protocol traffic.",0
ui_cancel_transfer: .byte "ESC cancels; partial file stays.",0
ui_results: .word ui_xok,ui_xerror,ui_xcancel,ui_xwire,ui_xdisk,ui_xbinary,ui_xbank,ui_xresume
ui_xok: .byte "Transfer complete.",0
ui_xerror: .byte "Transfer failed or timed out.",0
ui_xcancel: .byte "Transfer cancelled; partial kept.",0
ui_xwire: .byte "Connection I/O error.",0
ui_xdisk: .byte "File I/O error.",0
ui_xbinary: .byte "Peer must enable binary Telnet.",0
ui_xbank: .byte "Install matching D:TRANS.BIN.",0
ui_xresume: .byte "Resume prefix did not verify.",0
