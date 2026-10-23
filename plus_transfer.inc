; XMODEM-CRC (128/1024 receive, 1024 send) and single-file YMODEM.
; Stop-and-wait: verify CRC and commit disk bytes before ACK. Duplicates
; are ACKed without writing again. Every wait is bounded and ESC cancels.
xptr=$ca
.segment "BSS"
xmode: .res 1
xdirection: .res 1
xseq: .res 1
xpacket: .res 1
xlength: .res 2
xpos: .res 2
xcrc: .res 2
xwire_crc: .res 1
xretry: .res 1
xresult: .res 1
xclock: .res 1
xwire_len: .res 1
xwire_pos: .res 1
xbyte: .res 1
xremaining: .res 4
xtotal: .res 4
xheader: .res 1
xstarted: .res 1
xheader_bad: .res 1
xended: .res 1
xbuf: .res 1024
xframe: .res 3
xtrail: .res 2
.segment "CODE"
.proc xcrc_byte
 eor xcrc+1
 tax
 lda xcrc
 eor crc_hi,x
 sta xcrc+1
 lda crc_lo,x
 sta xcrc
 rts
.endproc
.proc xcrc_block
 lda #0
 sta xcrc
 sta xcrc+1
 sta xpos
 sta xpos+1
 lda #<xbuf
 sta xptr
 lda #>xbuf
 sta xptr+1
@loop: ldy #0
 lda (xptr),y
 jsr xcrc_byte
 inc xptr
 bne :+
 inc xptr+1
: inc xpos
 bne :+
 inc xpos+1
: lda xpos
 cmp xlength
 bne @loop
 lda xpos+1
 cmp xlength+1
 bne @loop
 rts
.endproc

; Get a wire byte with a three-second deadline. Burst reads amortize SIO cost.
.proc wire_raw_get
 lda $14
 sta xclock
@wait:
 lda menu_key_ready
 beq :+
 lda menu_key
 cmp #28
 jeq @cancel
: lda xwire_len
 jne @available
 lda device_type
 beq @rstatus
 jsr n_status_sio
 lda DSTATS
 jmi @failed
 lda DVSTAT0
 ldx DVSTAT1
 jmp @count
@rstatus:
 ldx #$10
 lda #13
 sta ICCOM+$10
 jsr CIOV
 jmi @failed
 lda DVSTAT1
 ldx DVSTAT2
@count:
 cpx #0
 beq :+
 lda #255
: cmp #0
 beq @timeout
 sta xwire_len
 lda #0
 sta xwire_pos
 lda device_type
 beq @rread
 lda #FUJI_ID
 sta DDEVIC
 lda n_unit
 sta DUNIT
 lda #'R'
 sta DCOMND
 lda #$40
 sta DSTATS
 lda #<recv_buffer
 sta DBUFLO
 lda #>recv_buffer
 sta DBUFHI
 lda xwire_len
 sta DBYTLO
 sta DAUX1
 lda #0
 sta DBYTHI
 sta DAUX2
 lda #FUJI_TIMEOUT
 sta DTIMLO
 jsr SIOV
 lda DSTATS
 bmi @failed
 jmp @available
@rread:
 ldx #$10
 lda #7
 sta ICCOM+$10
 lda #<recv_buffer
 sta ICBA+$10
 lda #>recv_buffer
 sta ICBA+$11
 lda xwire_len
 sta ICBL+$10
 lda #0
 sta ICBL+$11
 jsr CIOV
 bmi @failed
@available:
 ldx xwire_pos
 lda recv_buffer,x
 inc xwire_pos
 dec xwire_len
 pha
 jsr ui_rx_progress
 pla
 clc
 rts
@timeout:
 lda $14
 sec
 sbc xclock
 cmp #180
 jcc @wait
@failed:sec
 rts
@cancel:lda #2
 sta xresult
 sec
 rts
.endproc

; io_ptr/io_size identify a contiguous packet span.
.proc wire_raw_send
 lda device_type
 beq @r
 lda #FUJI_ID
 sta DDEVIC
 lda n_unit
 sta DUNIT
 lda #'W'
 sta DCOMND
 lda #$80
 sta DSTATS
 lda io_ptr
 sta DBUFLO
 lda io_ptr+1
 sta DBUFHI
 lda io_size
 sta DBYTLO
 sta DAUX1
 lda io_size+1
 sta DBYTHI
 sta DAUX2
 lda #FUJI_TIMEOUT
 sta DTIMLO
 jsr SIOV
 lda DSTATS
 bmi @error
 jsr ui_tx_progress
 clc
 rts
@r:
 ldx #$10
 lda #11
 sta ICCOM+$10
 lda io_ptr
 sta ICBA+$10
 lda io_ptr+1
 sta ICBA+$11
 lda io_size
 sta ICBL+$10
 lda io_size+1
 sta ICBL+$11
 jsr CIOV
 bmi @error
 jsr ui_tx_progress
 clc
 rts
@error:lda #3
 sta xresult
 sec
 rts
.endproc
.proc wire_byte
 sta xbyte
 lda #<xbyte
 sta io_ptr
 lda #>xbyte
 sta io_ptr+1
 lda #1
 sta io_size
 lda #0
 sta io_size+1
 jmp wire_send
.endproc

.proc xfer_binary_ready
 lda telnet_on
 beq @ok
 lda tn_enabled
 and tn_enabled+1
 and #1
 bne @ok
 lda #5
 sta xresult
 sec
 rts
@ok: clc
 rts
.endproc
.proc xfer_begin
 lda #0
 sta xwire_len
 sta xwire_pos
 sta xresult
 sta menu_key_ready
 sta xheader
 sta xstarted
 sta xended
 sta xremaining
 sta xremaining+1
 sta xremaining+2
 sta xremaining+3
 lda #1
 sta xseq
 rts
.endproc
.proc xfer_cancel
 lda #24
 jsr wire_byte
 lda #24
 jsr wire_byte
 jmp plus_file_close
.endproc

; Read packet body after SOH/STX, then sequence/complement and CRC.
; Returns C clear only on a valid packet. xpacket is its sequence number.
.proc xfer_packet
 cmp #1
 beq @small
 lda #0
 sta xlength
 lda #4
 bne @len
@small:lda #128
 sta xlength
 lda #0
@len:sta xlength+1
 jsr wire_get
 jcs @bad
 sta xpacket
 jsr wire_get
 jcs @bad
 eor xpacket
 eor #255
 sta xheader_bad
 lda #0
 sta xpos
 sta xpos+1
 lda #<xbuf
 sta xptr
 lda #>xbuf
 sta xptr+1
@read:jsr wire_get
 jcs @bad
 ldy #0
 sta (xptr),y
 inc xptr
 bne :+
 inc xptr+1
: inc xpos
 bne :+
 inc xpos+1
: lda xpos
 cmp xlength
 bne @read
 lda xpos+1
 cmp xlength+1
 bne @read
 jsr wire_get
 bcs @bad
 sta xwire_crc
 jsr wire_get
 bcs @bad
 sta xtrail+1
 lda xheader_bad
 bne @bad
 jsr xcrc_block
 lda xwire_crc
 cmp xcrc+1
 bne @bad
 lda xtrail+1
 cmp xcrc
 bne @bad
 clc
 rts
@bad:sec
 rts
.endproc

.proc xfer_receive
 jsr xfer_begin
 lda #8
 sta io_mode
 jsr plus_file_open
 lda io_error
 jeq @start
 lda #4
 sta xresult
 rts
@start:
 lda #10
 sta xretry
 lda #'C'
 jsr wire_byte
@next:jsr wire_get
 jcs @retry
 cmp #1
 beq @packet
 cmp #2
 beq @packet
 cmp #4
 jeq @eot
 cmp #24
 jeq @cancel
 jmp @retry
@packet:
 pha
 jsr xfer_binary_ready
 pla
 jcs @binaryfail
 pha
 lda #1
 sta xstarted
 pla
 jsr xfer_packet
 jcs @retry
 lda xmode
 beq @data
 lda xheader
 bne @data
 lda xpacket
 jne @retry
 lda xbuf
 jeq @finish
 lda xended
 jne @cancel
 jsr ymodem_size
 jcs @cancel
 lda #1
 sta xheader
 lda #10
 sta xretry
 lda #6
 jsr wire_byte
 lda #'C'
 jsr wire_byte
 jmp @next
@data:
 lda xpacket
 cmp xseq
 beq @write
 clc
 adc #1
 cmp xseq
 jne @retry
 lda #6
 jsr wire_byte
 lda xmode
 jeq @next
 lda xpacket
 jne @next
 lda xseq
 cmp #1
 jne @next
 lda #'C'
 jsr wire_byte
 jmp @next
@write:
 lda xlength
 sta io_size
 lda xlength+1
 sta io_size+1
 lda xmode
 beq @store
 lda xremaining+2
 ora xremaining+3
 bne @subtract
 lda xremaining+1
 cmp io_size+1
 bcc @short
 bne @subtract
 lda xremaining
 cmp io_size
 bcs @subtract
@short:
 lda xremaining
 sta io_size
 lda xremaining+1
 sta io_size+1
@subtract:
 lda xremaining
 sec
 sbc io_size
 sta xremaining
 lda xremaining+1
 sbc io_size+1
 sta xremaining+1
 lda xremaining+2
 sbc #0
 sta xremaining+2
 lda xremaining+3
 sbc #0
 sta xremaining+3
@store:
 lda #<xbuf
 sta io_ptr
 lda #>xbuf
 sta io_ptr+1
 lda io_size
 ora io_size+1
 beq @ack
 jsr plus_file_write
 lda io_error
 jne @diskfail
@ack:
 inc xseq
 lda #10
 sta xretry
 lda #6
 jsr wire_byte
 jmp @next
@retry:
 lda xresult
 jne @cancel
 dec xretry
 jeq @timeout
 lda #21
 ldx xstarted
 bne :+
 lda #'C'
: jsr wire_byte
 jmp @next
@eot:
 lda xmode
 beq @done
 lda xremaining
 ora xremaining+1
 ora xremaining+2
 ora xremaining+3
 jne @retry
 lda #1
 sta xended
 ; Standard YMODEM two-EOT handshake then the terminating empty header.
 lda #21
 jsr wire_byte
 jsr wire_get
 bcs @retry
 cmp #4
 bne @retry
 lda #6
 jsr wire_byte
 lda #'C'
 jsr wire_byte
 lda #0
 sta xheader
 jmp @next
@finish:
 lda #6
 jsr wire_byte
 jmp @closed
@done:
 lda #6
 jsr wire_byte
@closed:
 jsr plus_file_close
 lda io_error
 jne @diskfail
 lda #0
 sta xresult
 rts
@timeout:
 lda #1
 sta xresult
 jmp xfer_cancel
@diskfail:
 lda #4
 sta xresult
 jmp xfer_cancel
@cancel:
 lda #2
 sta xresult
@binaryfail:
 jmp xfer_cancel
.endproc

.proc ymodem_size
 ldy #0
@name:lda xbuf,y
 beq @start
 iny
 cpy #127
 bcc @name
 sec
 rts
@start:iny
 lda xbuf,y
 cmp #'0'
 jcc @invalid
 cmp #'9'+1
 jcs @invalid
@digit:
 lda xbuf,y
 cmp #'0'
 jcc @done
 cmp #'9'+1
 jcs @done
 sec
 sbc #'0'
 sta plus_digit
 ; Overflow guard for UINT32_MAX / 10 (429496729).
 lda xremaining+3
 cmp #$19
 bcc @multiply
 jne @invalid
 lda xremaining+2
 cmp #$99
 bcc @multiply
 jne @invalid
 lda xremaining+1
 cmp #$99
 bcc @multiply
 jne @invalid
 lda xremaining
 cmp #$99
 bcc @multiply
 jne @invalid
 lda plus_digit
 cmp #6
 bcs @invalid
@multiply:
 ; 32-bit multiply by ten with temporary total.
 ldx #3
@save:lda xremaining,x
 sta xtotal,x
 dex
 bpl @save
 asl xremaining
 rol xremaining+1
 rol xremaining+2
 rol xremaining+3
 asl xremaining
 rol xremaining+1
 rol xremaining+2
 rol xremaining+3
 clc
 lda xremaining
 adc xtotal
 sta xremaining
 lda xremaining+1
 adc xtotal+1
 sta xremaining+1
 lda xremaining+2
 adc xtotal+2
 sta xremaining+2
 lda xremaining+3
 adc xtotal+3
 sta xremaining+3
 asl xremaining
 rol xremaining+1
 rol xremaining+2
 rol xremaining+3
 clc
 lda xremaining
 adc plus_digit
 sta xremaining
 lda xremaining+1
 adc #0
 sta xremaining+1
 lda xremaining+2
 adc #0
 sta xremaining+2
 lda xremaining+3
 adc #0
 sta xremaining+3
 iny
 cpy #128
 jcc @digit
@invalid: sec
 rts
@done: clc
 rts
.endproc

.proc xfer_send_packet
 lda xlength+1
 beq :+
 lda #2
 bne :++
: lda #1
: sta xframe
 lda xseq
 sta xframe+1
 eor #255
 sta xframe+2
 jsr xcrc_block
 lda xcrc+1
 sta xtrail
 lda xcrc
 sta xtrail+1
 lda #10
 sta xretry
@again:
 lda #<xframe
 sta io_ptr
 lda #>xframe
 sta io_ptr+1
 lda #3
 sta io_size
 lda #0
 sta io_size+1
 jsr wire_send
 bcs @bad
 lda #<xbuf
 sta io_ptr
 lda #>xbuf
 sta io_ptr+1
 lda xlength
 sta io_size
 lda xlength+1
 sta io_size+1
 jsr wire_send
 bcs @bad
 lda #<xtrail
 sta io_ptr
 lda #>xtrail
 sta io_ptr+1
 lda #2
 sta io_size
 lda #0
 sta io_size+1
 jsr wire_send
 bcs @bad
 jsr wire_get
 bcs @retry
 cmp #6
 beq @ok
 cmp #24
 beq @bad
@retry:dec xretry
 bne @again
@bad:sec
 rts
@ok:clc
 rts
.endproc

.proc xfer_send
 jsr xfer_begin
 lda #4
 sta io_mode
 jsr plus_file_open
 lda io_error
 jne @diskfail
 ; YMODEM needs exact file length: bounded streaming pass, then reopen.
 lda xmode
 beq @sync
 lda #0
 ldx #3
@zero:sta xtotal,x
 dex
 bpl @zero
@measure:
 jsr xfer_read_block
 lda io_size
 clc
 adc xtotal
 sta xtotal
 lda io_size+1
 adc xtotal+1
 sta xtotal+1
 lda xtotal+2
 adc #0
 sta xtotal+2
 lda xtotal+3
 adc #0
 sta xtotal+3
 lda io_error
 beq @measure
 cmp #136
 jne @diskfail
 jsr plus_file_close
 jsr plus_file_open
 lda io_error
 jne @diskfail
@sync:
 lda #10
 sta xretry
@syncwait:jsr wire_get
 bcs @syncretry
 cmp #'C'
 beq @ready
 cmp #24
 jeq @cancel
@syncretry:dec xretry
 bne @syncwait
 jmp @cancel
@ready:
 jsr xfer_binary_ready
 jcs @binaryfail
 lda xmode
 beq @data
 jsr ymodem_header
 lda #0
 sta xseq
 lda #128
 sta xlength
 lda #0
 sta xlength+1
 jsr xfer_send_packet
 jcs @cancel
 jsr wire_get
 jcs @cancel
 cmp #'C'
 jne @cancel
 lda #1
 sta xseq
@data:
 jsr xfer_read_block
 lda io_size
 ora io_size+1
 jeq @eot
 lda io_error
 beq :+
 cmp #136
 jne @diskfail
: ; Pad unused tail with CPM EOF, including zero-length remainder.
 lda io_size
 sta xpos
 lda io_size+1
 sta xpos+1
 lda #<xbuf
 clc
 adc xpos
 sta xptr
 lda #>xbuf
 adc xpos+1
 sta xptr+1
@pad:
 lda xpos+1
 cmp #4
 bcs @send
 ldy #0
 lda #26
 sta (xptr),y
 inc xptr
 bne :+
 inc xptr+1
: inc xpos
 bne @pad
 inc xpos+1
 jmp @pad
@send:
 lda #0
 sta xlength
 lda #4
 sta xlength+1
 jsr xfer_send_packet
 jcs @cancel
 inc xseq
 jmp @data
@eot:
 lda #10
 sta xretry
@eotagain:
 lda #4
 jsr wire_byte
 jsr wire_get
 bcs @eotretry
 cmp #6
 beq @endheader
 cmp #24
 jeq @cancel
@eotretry:dec xretry
 bne @eotagain
 jmp @cancel
@endheader:
 lda xmode
 beq @done
 jsr wire_get
 jcs @cancel
 cmp #'C'
 jne @cancel
 ldx #127
 lda #0
@empty:sta xbuf,x
 dex
 bpl @empty
 sta xseq
 sta xlength+1
 lda #128
 sta xlength
 jsr xfer_send_packet
 jcs @cancel
@done:
 jsr plus_file_close
 lda io_error
 jne @diskfail
 lda #0
 sta xresult
 rts
@diskfail:
 lda #4
 sta xresult
 jmp xfer_cancel
@cancel:
 lda #2
 sta xresult
@binaryfail:
 jmp xfer_cancel
.endproc
.proc xfer_read_block
 lda #<xbuf
 sta io_ptr
 lda #>xbuf
 sta io_ptr+1
 lda #0
 sta io_size
 lda #4
 sta io_size+1
 jmp plus_file_read
.endproc
.proc ymodem_header
 lda #0
 ldx #127
@zero:sta xbuf,x
 dex
 bpl @zero
 ; Use Atari file's basename. Incoming filenames never select an output path.
 ldx #0
@colon:lda io_name,x
 cmp #':'
 beq @name
 inx
 cpx #63
 bcc @colon
 ldx #$ff
@name:inx
 ldy #0
@copy:lda io_name,x
 cmp #$9b
 beq @size
 sta xbuf,y
 inx
 iny
 cpy #63
 bcc @copy
@size:iny
 sty plus_j
 lda #0
 sta plus_count
@divide:
 lda #0
 ldx #32
@bit:
 asl xtotal
 rol xtotal+1
 rol xtotal+2
 rol xtotal+3
 rol
 cmp #10
 bcc :+
 sbc #10
 inc xtotal
: dex
 bne @bit
 pha
 inc plus_count
 lda xtotal
 ora xtotal+1
 ora xtotal+2
 ora xtotal+3
 bne @divide
 ldy plus_j
@digits:
 pla
 ora #'0'
 sta xbuf,y
 iny
 dec plus_count
 bne @digits
 rts
.endproc

tool_receive:
 lda #0
 sta xmode
 sta xdirection
 jmp transfer_dialog
tool_send:
 lda #0
 sta xmode
 lda #1
 sta xdirection
 jmp transfer_dialog
tool_yreceive:
 lda #1
 sta xmode
 lda #0
 sta xdirection
 jmp transfer_dialog
tool_ysend:
 lda #1
 sta xmode
 sta xdirection
.proc transfer_dialog
 lda dev_ready
 bne :+
 rts
: jsr plus_dialog_begin
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
 lda #<transfer_msg
 ldx #>transfer_msg
 jsr print_str
 lda #17
 jsr plus_queue
 jsr check_sendbuf
 lda xmode
 cmp #2
 beq @zmodem
 cmp #4
 beq @zmodem
 cmp #3
 beq @kermit
 lda xdirection
 beq @receive
 jsr xfer_send
 jmp @result
@receive:jsr xfer_receive
 jmp @result
@zmodem:
 lda xdirection
 beq :+
 jsr zmodem_send
 jmp @result
: jsr zmodem_receive
 jmp @result
@kermit:
 lda xdirection
 beq :+
 jsr kermit_send
 jmp @result
: jsr kermit_receive
@result:
 lda xresult
 beq @ok
 cmp #5
 bne :+
 lda #<transfer_binary_error
 ldx #>transfer_binary_error
 jsr plus_message
 jmp @end
: cmp #6
 bne :+
 lda #<transfer_bank_error
 ldx #>transfer_bank_error
 jsr plus_message
 jmp @end
: cmp #7
 bne :+
 lda #<transfer_resume_error
 ldx #>transfer_resume_error
 jsr plus_message
 jmp @end
: lda #<transfer_error
 ldx #>transfer_error
 jsr plus_message
 jmp @end
@ok:
 lda #<transfer_ok
 ldx #>transfer_ok
 jsr plus_message
@end: jmp plus_dialog_end
.endproc
transfer_msg: .byte "Transferring; ESC cancels.",13,10,$9b
transfer_ok: .byte "Transfer complete.",$9b
transfer_binary_error: .byte "Peer did not enable binary Telnet.",13,10,"Use raw TCP or a binary-capable host.",$9b
transfer_bank_error: .byte "D:TRANS.BIN missing or unreadable.",13,10,"Install the matching bank and restart.",$9b
transfer_resume_error: .byte "Resume prefix could not be verified.",13,10,"Existing file was not changed.",$9b
transfer_error: .byte "Transfer stopped (timeout, cancel or I/O error).",13,10,"Receive file may be partial.",$9b
.include "crc_tables.inc"
