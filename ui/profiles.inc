.proc ui_read_profile
 ; ui_index is a slot number 0..7.
 lda memac_bank_sel
 pha
 lda #$9e
 sta memac_bank_sel
 lda #0
 sta ui_ptr
 lda ui_index
 asl
 clc
 adc #$a0
 sta ui_ptr+1
 ldy #0
@first:lda (ui_ptr),y
 sta profile_rec,y
 iny
 bne @first
 inc ui_ptr+1
@second:lda (ui_ptr),y
 sta profile_rec+256,y
 iny
 bne @second
 pla
 sta memac_bank_sel
 lda #$9b
 sta profile_rec+31
 sta profile_rec+287
 sta profile_rec+351
 sta profile_rec+479
 ldx #3
@magic:lda profile_rec+508,x
 cmp ui_magic,x
 bne @invalid
 dex
 bpl @magic
 clc
 rts
@invalid:sec
 rts
.endproc
.proc ui_shadow
 ; ui_arg=0 saves draft, 1 restores it. VRAM $1d000 is scratch, not a profile.
 lda memac_bank_sel
 pha
 lda #$9d
 sta memac_bank_sel
 ldy #0
 lda ui_arg
 bne @restore
@save:lda profile_rec,y
 sta $a000,y
 lda profile_rec+256,y
 sta $a100,y
 iny
 bne @save
 beq @done
@restore:lda $a000,y
 sta profile_rec,y
 lda $a100,y
 sta profile_rec+256,y
 iny
 bne @restore
@done:pla
 sta memac_bank_sel
 rts
.endproc
.proc ui_new_profile
 lda #0
 tax
@clear:sta profile_rec,x
 sta profile_rec+256,x
 inx
 bne @clear
 lda #$9b
 sta profile_rec
 sta profile_rec+32
 sta profile_rec+288
 sta profile_rec+352
 sta server_buf
 sta port_buf
 lda #'T'
 sta profile_rec+480
 lda #8
 sta ui_save_slot
 lda #0
 sta ui_selected
 sta ui_field
 jmp ui_record_settings
.endproc
.proc ui_record_settings
 lda dialog_core+(utf8_mode-plus_state_start)
 sta profile_rec+481
 lda dialog_atascii
 sta profile_rec+482
 lda term_last_col
 sta profile_rec+483
 lda active_font
 sta profile_rec+484
 lda serial_baud_code
 sta profile_rec+485
 ldx #3
@magic:lda ui_magic,x
 sta profile_rec+508,x
 dex
 bpl @magic
 rts
.endproc
.proc ui_current_record
 jsr ui_new_profile
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
 lda device_type
 bne @done
 lda #'R'
 sta profile_rec+32
 lda #':'
 sta profile_rec+33
 lda #$9b
 sta profile_rec+34
@done:rts
.endproc

.proc ui_directory_loop
 ui_print ui_directory_title
 ui_at 5,5,UI_BORDER
 lda ui_width
 cmp #40
 beq :+
 ui_print ui_dir_header
 jmp @rows
: ui_print ui_dir_header40
@rows:lda #0
 sta ui_index
@slot:
 jsr ui_read_profile
 php
 lda ui_index
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
@blank:lda #' '
 jsr ui_put
 lda ui_x
 cmp ui_right
 bcc @blank
 lda #5
 sta ui_x
 lda ui_index
 clc
 adc #'1'
 jsr ui_put
 lda #8
 sta ui_x
 plp
 bcs @empty
 lda ui_width
 cmp #40
 beq :+
 lda #26
 sta ui_limit
: ui_print profile_rec
 lda ui_width
 sta ui_limit
 cmp #40
 beq @next
 lda #28
 sta ui_x
 lda ui_right
 sta ui_limit
 ui_print profile_rec+32
 lda ui_width
 sta ui_limit
 jmp @next
@empty:ui_print ui_empty_slot
@next:inc ui_index
 lda ui_index
 cmp #8
 jcc @slot
 ui_at 17,5,UI_HINT
 lda ui_profile_mode
 beq :+
 ui_print ui_select_save_slot
 jmp @hints
: ui_print ui_directory_help
@hints:ui_at 18,5,UI_HINT
 ui_print ui_directory_help2
@key:jsr ui_refresh
 jsr ui_key
 cmp #27
 jeq ui_form_back
 cmp #$80
 beq @up
 cmp #'-'
 beq @up
 cmp #$81
 beq @down
 cmp #'='
 beq @down
 cmp #'1'
 bcc @letters
 cmp #'9'
 bcs @letters
 sec
 sbc #'1'
 sta ui_selected
 jmp ui_redraw
@letters:
 cmp #13
 jeq @choose
 and #$df
 cmp #'E'
 jeq @edit
 cmp #'N'
 jeq @new
 jmp @key
@up:lda ui_selected
 beq @key
 dec ui_selected
 jmp ui_redraw
@down:lda ui_selected
 cmp #7
 bcs @key
 inc ui_selected
 jmp ui_redraw
@choose:
 lda ui_selected
 sta ui_save_slot
 lda ui_profile_mode
 beq @connect
 cmp #2
 beq @store_draft
 lda ui_selected
 pha
 jsr ui_current_record
 pla
 sta ui_save_slot
 jsr ui_decode_url
 jmp @edit_page
@store_draft:lda #1
 sta ui_arg
 jsr ui_shadow
 jsr ui_store_profile
 jmp ui_form_back
@connect:
 lda ui_selected
 sta ui_index
 jsr ui_read_profile
 jcs @key
 jsr ui_connect_record
 lda ui_action
 beq @redraw
 rts
@edit:
 lda ui_selected
 sta ui_index
 sta ui_save_slot
 jsr ui_read_profile
 bcs @new
 jsr ui_decode_url
 jmp @edit_page
@new:lda ui_selected
 pha
 jsr ui_new_profile
 pla
 sta ui_save_slot
@edit_page:lda #3
 sta ui_page
 lda #0
 sta ui_selected
 sta ui_field
@redraw:jmp ui_redraw
.endproc

.proc ui_decode_url
 lda #$9b
 sta server_buf
 sta port_buf
 lda profile_rec+32
 cmp #'N'
 bne @out
 ldx #0
@slashes:lda profile_rec+32,x
 cmp #$9b
 beq @out
 cmp #'/'
 beq @host_start
 inx
 cpx #250
 bcc @slashes
 rts
@host_start:inx
 inx
 ldy #0
@host:lda profile_rec+32,x
 cmp #$9b
 beq @end_host
 cmp #':'
 beq @port
 sta server_buf,y
 inx
 iny
 cpy #63
 bcc @host
@end_host:lda #$9b
 sta server_buf,y
 rts
@port:lda #$9b
 sta server_buf,y
 inx
 ldy #0
@copy_port:lda profile_rec+32,x
 cmp #$9b
 beq @end_port
 sta port_buf,y
 inx
 iny
 cpy #7
 bcc @copy_port
@end_port:lda #$9b
 sta port_buf,y
@out:rts
.endproc

.proc ui_profile_loop
 ui_print ui_edit_profile_title
 lda #0
 sta ui_index
@field:
 lda ui_index
 asl
 clc
 adc #5
 sta ui_y
 lda #4
 sta ui_x
 lda #UI_NORMAL
 ldx ui_index
 cpx ui_field
 bne :+
 lda #UI_HIGHLIGHT
: sta ui_color
 lda ui_profile_labels_lo,x
 pha
 lda ui_profile_labels_hi,x
 tax
 pla
 jsr ui_text
 lda #16
 sta ui_x
 lda ui_right
 sta ui_limit
 lda ui_index
 cmp #1
 beq @protocol
 jsr ui_profile_pointer
 lda ui_edit_ptr
 ldx ui_edit_ptr+1
 jsr ui_text
 jmp @next
@protocol:jsr ui_protocol_text
@next:lda ui_width
 sta ui_limit
 inc ui_index
 lda ui_index
 cmp #6
 bcc @field
 ui_at 18,4,UI_ACCENT
 ui_print ui_profile_buttons
@key:jsr ui_refresh
 jsr ui_key
 cmp #27
 jeq ui_form_back
 cmp #13
 beq @edit
 cmp #$80
 beq @up
 cmp #'-'
 beq @up
 cmp #$81
 beq @down
 cmp #'='
 beq @down
 and #$df
 cmp #'S'
 jeq @save
 cmp #'C'
 jeq @connect
 jmp @key
@up:lda ui_field
 beq @key
 dec ui_field
 jmp ui_redraw
@down:lda ui_field
 cmp #5
 bcs @key
 inc ui_field
 jmp ui_redraw
@edit:lda ui_field
 cmp #1
 beq @cycle_protocol
 sta ui_index
 jsr ui_profile_pointer
 lda ui_field
 asl
 clc
 adc #6
 sta ui_edit_y
 lda #0
 sta ui_edit_mask
 jsr ui_edit
 jmp ui_redraw
@cycle_protocol:
 lda profile_rec+32
 cmp #'R'
 jeq @key
 lda profile_rec+480
 cmp #'T'
 beq @raw
 cmp #'R'
 beq @ssh
 lda #'T'
 bne @setproto
@raw:lda #'R'
 bne @setproto
@ssh:lda #'S'
@setproto:sta profile_rec+480
 jmp ui_redraw
@save:
 jsr ui_build_url
 jcs ui_redraw
 lda ui_save_slot
 cmp #8
 bcc @store
 lda #0
 sta ui_arg
 jsr ui_shadow
 lda #2
 sta ui_profile_mode
 lda #1
 sta ui_page
 lda #0
 sta ui_selected
 jmp ui_redraw
@store:jsr ui_store_profile
 jmp ui_redraw
@connect:
 jsr ui_build_url
 jcs ui_redraw
 jsr ui_connect_record
 lda ui_action
 jeq ui_redraw
 rts
.endproc
.proc ui_profile_pointer
 ldx ui_index
 lda ui_profile_fields_lo,x
 sta ui_edit_ptr
 lda ui_profile_fields_hi,x
 sta ui_edit_ptr+1
 lda ui_profile_max,x
 sta ui_edit_max
 rts
.endproc
.proc ui_protocol_text
 lda profile_rec+32
 cmp #'R'
 bne :+
 ui_print ui_serial_profile
 rts
:
 lda profile_rec+480
 cmp #'S'
 beq @ssh
 cmp #'R'
 beq @raw
 ui_print ui_telnet_label
 rts
@raw:ui_print ui_raw_label
 rts
@ssh:ui_print ui_ssh_label
 rts
.endproc
.proc ui_build_url
 lda profile_rec+32
 cmp #'R'
 bne :+
 clc
 rts
: jsr ui_validate_port
 jcs @bad
 lda server_buf
 cmp #$9b
 jeq @bad
 jeq @bad
 cmp #0
 jeq @bad
 ldx #0
 lda #<ui_n_prefix
 sta ui_str
 lda #>ui_n_prefix
 sta ui_str+1
 lda profile_rec+480
 cmp #'S'
 bne :+
 lda #<ui_s_prefix
 sta ui_str
 lda #>ui_s_prefix
 sta ui_str+1
: ldy #0
@prefix:lda (ui_str),y
 beq @host
 sta profile_rec+32,x
 inx
 iny
 bne @prefix
@host:ldy #0
@hostloop:lda server_buf,y
 cmp #$9b
 beq @port
 cmp #33
 bcc @bad
 sta profile_rec+32,x
 inx
 iny
 cpy #63
 bcc @hostloop
@port:lda #':'
 sta profile_rec+32,x
 inx
 lda port_buf
 cmp #$9b
 bne @port_copy
 lda #'2'
 sta profile_rec+32,x
 inx
 lda #'3'
 ldy profile_rec+480
 cpy #'S'
 bne :+
 lda #'2'
: sta profile_rec+32,x
 inx
 jmp @done
@port_copy:ldy #0
@portloop:lda port_buf,y
 cmp #$9b
 beq @done
 cmp #'0'
 bcc @bad
 cmp #'9'+1
 bcs @bad
 sta profile_rec+32,x
 inx
 iny
 cpy #5
 bcc @portloop
 lda port_buf,y
 cmp #$9b
 bne @bad
@done:lda #$9b
 sta profile_rec+32,x
 clc
 rts
@bad:lda #<ui_bad_address
 ldx #>ui_bad_address
 jsr ui_notice
 sec
 rts
.endproc
.proc ui_validate_port
 ; Empty selects the protocol default. Otherwise accept only 1..65535.
 lda port_buf
 cmp #$9b
 beq @valid
 ldy #0
 lda #0
 sta ui_tmp
@digit:
 lda port_buf,y
 cmp #$9b
 beq @end
 cmp #'0'
 bcc @bad
 cmp #'9'+1
 bcs @bad
 sec
 sbc #'0'
 ora ui_tmp
 sta ui_tmp
 iny
 cpy #6
 bcc @digit
@bad:sec
 rts
@end:lda ui_tmp
 beq @bad
 cpy #5
 bcc @valid
 ldy #0
@max:lda port_buf,y
 cmp ui_max_port,y
 bcc @valid
 bne @bad
 iny
 cpy #5
 bcc @max
@valid:clc
 rts
.endproc
ui_max_port: .byte "65535"
.proc ui_store_profile
 lda profile_rec
 beq @default_name
 cmp #$9b
 bne @named
@default_name:
 lda profile_rec+32
 cmp #'R'
 beq @serial_name
 ldx #0
@name_host:lda server_buf,x
 cmp #$9b
 beq @name_end
 sta profile_rec,x
 inx
 cpx #31
 bcc @name_host
@name_end:lda #$9b
 sta profile_rec,x
 jmp @named
@serial_name:ldx #0
@name_serial:lda ui_serial_profile,x
 beq @name_end
 sta profile_rec,x
 inx
 bne @name_serial
@named:
 ; Draft remains in profile_rec. Inspect target signature without replacing it.
 lda memac_bank_sel
 pha
 lda #$9e
 sta memac_bank_sel
 lda ui_save_slot
 asl
 clc
 adc #$a0
 sta ui_ptr+1
 lda #0
 sta ui_ptr
 ldy #0
 lda (ui_ptr),y
 sta ui_tmp
 pla
 sta memac_bank_sel
 lda ui_tmp
 beq @write
 lda #<ui_replace_profile
 ldx #>ui_replace_profile
 jsr ui_notice
 and #$df
 cmp #'Y'
 jne @out
@write:
 lda memac_bank_sel
 pha
 lda #$9e
 sta memac_bank_sel
 lda ui_save_slot
 asl
 clc
 adc #$a0
 sta ui_ptr+1
 lda #0
 sta ui_ptr
 ldy #0
@first:lda profile_rec,y
 sta (ui_ptr),y
 iny
 bne @first
 inc ui_ptr+1
@second:lda profile_rec+256,y
 sta (ui_ptr),y
 iny
 bne @second
 lda #<ui_directory_path
 ldx #>ui_directory_path
 ; Pointer is in the UI bank: copy filename directly before disabling the bank.
 sta ui_str
 stx ui_str+1
 ldy #0
@name:lda (ui_str),y
 sta io_name,y
 iny
 cmp #$9b
 bne @name
 lda #8
 sta io_mode
 jsr ui_file_open
 lda io_error
 bne @error
 lda #0
 sta io_ptr
 sta io_size
 lda #$a0
 sta io_ptr+1
 lda #16
 sta io_size+1
 jsr ui_file_write
 lda io_error
 pha
 jsr ui_file_close
 pla
 ora io_error
 sta ui_error
 pla
 sta memac_bank_sel
 lda ui_error
 bne @notice_error
 lda #<ui_saved
 ldx #>ui_saved
 jmp ui_notice
@error:pla
 sta memac_bank_sel
@notice_error:
 lda #<ui_profile_disk_error
 ldx #>ui_profile_disk_error
 jmp ui_notice
@out:rts
.endproc
.proc ui_connect_record
 lda dev_ready
 beq @connect
 lda #<ui_already_connected
 ldx #>ui_already_connected
 jmp ui_notice
@connect:
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
 lda profile_rec+480
 sta proto_byte
 lda #1
 sta profile_ready
 lda #2
 sta ui_action
 rts
.endproc
ui_magic: .byte "VBX1"
ui_directory_title: .byte " CONNECTION DIRECTORY ",0
ui_dir_header: .byte "#  NAME                ADDRESS",0
ui_dir_header40: .byte "#  SAVED CONNECTION",0
ui_empty_slot: .byte "<empty>",0
ui_select_save_slot: .byte "Choose a slot; RETURN saves.",0
ui_directory_help: .byte "RETURN Connect  E Edit  N New",0
ui_directory_help2: .byte "1-8 or arrows select / ESC Back",0
ui_edit_profile_title: .byte " CONNECTION DETAILS ",0
ui_profile_buttons: .byte "C Connect  S Save  ESC Cancel",0
ui_profile_labels_lo: .byte <ui_name_label,<ui_proto_label,<ui_host_label,<ui_port_label,<ui_user_label,<ui_macro_label
ui_profile_labels_hi: .byte >ui_name_label,>ui_proto_label,>ui_host_label,>ui_port_label,>ui_user_label,>ui_macro_label
ui_profile_fields_lo: .byte <profile_rec,0,<server_buf,<port_buf,<(profile_rec+288),<(profile_rec+352)
ui_profile_fields_hi: .byte >profile_rec,0,>server_buf,>port_buf,>(profile_rec+288),>(profile_rec+352)
ui_profile_max: .byte 31,0,63,5,63,127
ui_name_label: .byte "Name",0
ui_proto_label: .byte "Protocol",0
ui_host_label: .byte "Host",0
ui_port_label: .byte "Port",0
ui_user_label: .byte "User (SSH)",0
ui_macro_label: .byte "Login macro",0
ui_telnet_label: .byte "Telnet",0
ui_raw_label: .byte "Raw TCP",0
ui_ssh_label: .byte "SSH",0
ui_serial_profile: .byte "Serial (R:)",0
ui_n_prefix: .byte "N1:TCP://",0
ui_s_prefix: .byte "N1:SSH://",0
ui_bad_address: .byte "Host required; port 1-65535.",0
ui_replace_profile: .byte "Replace profile? Y=yes ESC=no",0
ui_directory_path: .byte "D:VBXE.DIR",$9b
ui_profile_disk_error: .byte "Saved in RAM; disk write failed.",0
ui_already_connected: .byte "Disconnect the current session.",0
