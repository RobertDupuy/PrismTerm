; Kermit basic packets, all required transactions, binary eight-bit quoting,
; repeat compression, negotiated prefix/EOL/padding, exact lengths and retries.
; Stop-and-wait (no long packets or windows advertised). Block check type 1.
k_tx = xbuf
k_rx = xbuf+128
k_data = xbuf+256
k_seq: .byte 0
k_type: .byte 0
k_rseq: .byte 0
k_rtype: .byte 0
k_len: .byte 0
k_rlen: .byte 0
k_sum: .byte 0
k_i: .byte 0
k_j: .byte 0
k_max: .byte 90
k_eol: .byte 13
k_qctl: .byte '#'
k_qbin: .byte 0
k_repeat: .byte 0
k_npad: .byte 0
k_pad: .byte 0
k_data_len: .byte 0
k_data_pos: .byte 0
k_eof: .byte 0
k_byte: .byte 0
k_high: .byte 0
k_count: .byte 0
k_state: .byte 0
k_tries: .byte 0
k_txsize: .byte 0
k_params: .byte 94+32,5+32,32,64,13+32,'#','&','1',32,32

.proc k_init
 jsr b_begin
 lda #0
 sta k_seq
 sta k_state
 sta k_data_len
 sta k_data_pos
 sta k_eof
 sta k_qbin
 sta k_repeat
 sta k_npad
 sta k_pad
 lda #90
 sta k_max
 lda #13
 sta k_eol
 lda #'#'
 sta k_qctl
 rts
.endproc
; The peer's QCTL describes how the peer will encode data; our outgoing
; prefix remains '#'. Eight-bit and repeat quoting are enabled by agreement.
.proc k_negotiate
 lda k_rlen
 beq @out
 lda k_rx
 sec
 sbc #32+3
 cmp #3
 bcc @out
 cmp #91
 bcc :+
 lda #91
: sta k_max
 lda k_rlen
 cmp #3
 bcc @out
 lda k_rx+2
 sec
 sbc #32
 cmp #95
 bcs @out
 sta k_npad
 lda k_rlen
 cmp #4
 bcc @out
 lda k_rx+3
 eor #64
 sta k_pad
 lda k_rlen
 cmp #5
 bcc @out
 lda k_rx+4
 sec
 sbc #32
 cmp #32
 bcs :+
 sta k_eol
: lda k_rlen
 cmp #6
 bcc @out
 lda k_rx+5
 cmp #33
 bcc :+
 cmp #127
 bcs :+
 sta k_qctl
: lda k_rlen
 cmp #7
 bcc @out
 lda k_rx+6
 cmp #'Y'
 beq @bin
 cmp #'&'
 bne :+
@bin: lda #'&'
 sta k_qbin
: lda k_rlen
 cmp #9
 bcc @out
 ; Repeat compression is not negotiated: bounded 256-byte decode buffer.
@out:rts
.endproc
.proc k_sum_end
 lda k_sum
 and #$c0
 lsr
 lsr
 lsr
 lsr
 lsr
 lsr
 clc
 adc k_sum
 and #63
 clc
 adc #32
 rts
.endproc
; Build in resident RAM, so a bank switch during transport cannot hide it.
.proc k_packet
 lda #1
 sta k_tx
 lda k_len
 clc
 adc #35
 sta k_tx+1
 lda k_seq
 clc
 adc #32
 sta k_tx+2
 lda k_type
 sta k_tx+3
 lda #0
 sta k_sum
 ldx #1
 lda k_len
 clc
 adc #4
 sta k_txsize
@sum: lda k_tx,x
 clc
 adc k_sum
 sta k_sum
 inx
 cpx k_txsize
 bcc @sum
 jsr k_sum_end
 ldx k_txsize
 sta k_tx,x
 inx
 lda k_eol
 sta k_tx,x
 inx
 stx k_txsize
 rts
.endproc
.proc k_send_packet
 jsr k_packet
 jmp k_resend
.endproc
.proc k_resend
 lda k_npad
 beq @packet
 sta k_i
@pad:
 lda k_pad
 sta xbyte
 lda #<xbyte
 sta io_ptr
 lda #>xbyte
 sta io_ptr+1
 lda #1
 sta io_size
 lda #0
 sta io_size+1
 jsr b_send
 bcs @out
 dec k_i
 bne @pad
@packet:
 lda #<k_tx
 sta io_ptr
 lda #>k_tx
 sta io_ptr+1
 lda k_txsize
 sta io_size
 lda #0
 sta io_size+1
 jsr b_send
@out:rts
.endproc
; A complete packet is bounded by its advertised length; all fields checked.
.proc k_get_packet
 lda #0
 sta k_i
@mark: jsr b_get
 jcs @bad
 and #127
 cmp #1
 beq @length
 inc k_i
 bne @mark
 jmp @bad
@length:
 jsr b_get
 jcs @bad
 and #127
 sta k_sum
 sec
 sbc #32
 cmp #3
 jcc @bad
 cmp #95
 jcs @bad
 sec
 sbc #3
 sta k_rlen
 jsr b_get
 jcs @bad
 and #127
 pha
 clc
 adc k_sum
 sta k_sum
 pla
 sec
 sbc #32
 cmp #64
 jcs @bad
 sta k_rseq
 jsr b_get
 jcs @bad
 and #127
 sta k_rtype
 clc
 adc k_sum
 sta k_sum
 lda #0
 sta k_i
@data:
 lda k_i
 cmp k_rlen
 beq @check
 jsr b_get
 jcs @bad
 ldx k_i
 sta k_rx,x
 clc
 adc k_sum
 sta k_sum
 inc k_i
 bne @data
@check:
 jsr b_get
 bcs @bad
 and #127
 sta k_byte
 jsr k_sum_end
 cmp k_byte
 bne @bad
 clc
 rts
@bad:sec
 rts
.endproc
.proc k_exchange
 jsr k_packet
 lda #10
 sta k_tries
@retry:
 jsr k_resend
 bcs @bad
 jsr k_get_packet
 bcs @again
 lda k_rtype
 cmp #'E'
 beq @bad
 cmp #'Y'
 bne @again
 lda k_rseq
 cmp k_seq
 beq @ok
@again:
 lda xresult
 bne @bad
 dec k_tries
 bne @retry
 lda #1
 sta xresult
@bad:sec
 rts
@ok:clc
 rts
.endproc
.proc k_next
 inc k_seq
 lda k_seq
 and #63
 sta k_seq
 rts
.endproc
.proc k_empty
 sta k_type
 lda #0
 sta k_len
 rts
.endproc
.proc k_init_data
 ldx #9
@copy:lda k_params,x
 sta k_tx+4,x
 dex
 bpl @copy
 lda #10
 sta k_len
 rts
.endproc
.proc k_close_error
 lda xresult
 bne :+
 lda #2
 sta xresult
: lda #'E'
 jsr k_empty
 jsr k_send_packet
 jmp b_close
.endproc
.proc k_disk_error
 lda #4
 sta xresult
 jmp k_close_error
.endproc
.proc k_receive
 jsr k_init
 lda #10
 sta k_tries
@next:
 jsr k_get_packet
 jcs @retry
 lda k_rtype
 cmp #'E'
 jeq k_close_error
 lda k_rseq
 cmp k_seq
 beq @dispatch
 clc
 adc #1
 and #63
 cmp k_seq
 jne @retry
 ; Repeat last ACK, including the S-ACK negotiation data.
 jsr k_resend
 jmp @next
@dispatch:
 lda k_rtype
 cmp #'S'
 beq @start
 cmp #'F'
 beq @file
 cmp #'D'
 jeq @data
 cmp #'Z'
 jeq @eof
 cmp #'B'
 jeq @end
 jmp @retry
@start:
 lda k_state
 jne @retry
 jsr k_negotiate
 jsr k_init_data
 lda #1
 sta k_state
 jmp @ack
@file:
 lda k_state
 cmp #1
 jne @retry
 lda #8
 sta io_mode
 jsr b_open
 lda io_error
 jne k_disk_error
 lda #2
 sta k_state
 jmp @emptyack
@data:
 lda k_state
 cmp #2
 jne @retry
 jsr k_decode
 jcs @retry
 lda #<k_data
 sta io_ptr
 lda #>k_data
 sta io_ptr+1
 lda k_data_len
 sta io_size
 lda #0
 sta io_size+1
 lda io_size
 beq @emptyack
 jsr b_write
 lda io_error
 jne k_disk_error
 jmp @emptyack
@eof:
 lda k_state
 cmp #2
 jne @retry
 ; A discard request is a canceled transfer, never success.
 lda k_rlen
 beq :+
 lda k_rx
 cmp #'D'
 jeq k_close_error
: jsr b_close
 lda io_error
 jne k_disk_error
 lda #3
 sta k_state
 jmp @emptyack
@end:
 lda k_state
 cmp #3
 beq :+
 cmp #1
 jne @retry
: lda #4
 sta k_state
@emptyack:lda #0
 sta k_len
@ack:
 lda #'Y'
 sta k_type
 jsr k_send_packet
 jcs k_close_error
 jsr k_next
 lda #10
 sta k_tries
 lda k_state
 cmp #4
 jne @next
 lda #0
 sta xresult
 rts
@retry:
 lda xresult
 jne k_close_error
 dec k_tries
 beq @timeout
 ; NAK does not overwrite the saved last ACK.
 lda k_txsize
 pha
 ldx #99
@save:lda k_tx,x
 sta k_data,x
 dex
 bpl @save
 lda #'N'
 jsr k_empty
 jsr k_send_packet
 ldx #99
@restore:lda k_data,x
 sta k_tx,x
 dex
 bpl @restore
 pla
 sta k_txsize
 jmp @next
@timeout:
 lda #1
 sta xresult
 jmp k_close_error
.endproc
; Decode into a 256-byte staging buffer. Expansion is bounded before writing.
.proc k_decode
 lda #0
 sta k_i
 sta k_data_len
@next:
 lda k_i
 cmp k_rlen
 jeq @ok
 lda #1
 sta k_count
 jsr @take
 jcs @bad
 ldx k_repeat
 beq @bin
 cmp k_repeat
 bne @bin
 jsr @take
 jcs @bad
 sec
 sbc #32
 cmp #2
 jcc @bad
 cmp #95
 bcs @bad
 sta k_count
 jsr @take
 bcs @bad
@bin:
 ldx #0
 stx k_high
 ldx k_qbin
 beq @ctl
 cmp k_qbin
 bne @ctl
 lda #128
 sta k_high
 jsr @take
 bcs @bad
@ctl:
 cmp k_qctl
 bne @store
 jsr @take
 bcs @bad
 sta k_byte
 and #127
 cmp #63
 bcc @literal
 cmp #96
 bcs @literal
 lda k_byte
 eor #64
 jmp @store
@literal:lda k_byte
@store:
 ora k_high
 sta k_byte
 lda k_data_len
 clc
 adc k_count
 bcs @bad
 sta k_j
 ldx k_data_len
 lda k_byte
@repeat:sta k_data,x
 inx
 cpx k_j
 bne @repeat
 stx k_data_len
 jmp @next
@take:
 ldx k_i
 cpx k_rlen
 bcs @bad
 lda k_rx,x
 inc k_i
 clc
 rts
@ok:clc
 rts
@bad:sec
 rts
.endproc
.proc k_send
 jsr k_init
 lda #4
 sta io_mode
 jsr b_open
 lda io_error
 jne k_disk_error
 lda #'S'
 sta k_type
 jsr k_init_data
 jsr k_exchange
 jcs k_close_error
 jsr k_negotiate
 jsr k_next
 lda #'F'
 sta k_type
 lda #0
 sta k_len
 ; Safe basename supplied by the local user, no remote path interpretation.
 ldx #0
@colon:lda io_name,x
 cmp #':'
 beq @name
 inx
 cpx #63
 bcc @colon
 ldx #$ff
@name:inx
 lda io_name,x
 cmp #$9b
 beq @file
 ldy k_len
 cpy k_max
 bcs @file
 sta k_tx+4,y
 inc k_len
 jmp @name
@file:jsr k_exchange
 jcs k_close_error
 jsr k_next
@data:
 jsr k_encode
 jcs k_disk_error
 lda k_len
 beq @eof
 lda #'D'
 sta k_type
 jsr k_exchange
 jcs k_close_error
 jsr k_next
 jmp @data
@eof:
 lda #'Z'
 jsr k_empty
 jsr k_exchange
 jcs k_close_error
 jsr k_next
 lda #'B'
 jsr k_empty
 jsr k_exchange
 jcs k_close_error
 jsr b_close
 lda io_error
 jne k_disk_error
 lda #0
 sta xresult
 rts
.endproc
; Build one packet while keeping a disk read ahead buffer. Worst case is
; three encoded bytes per source byte. No per-byte CIO/SIO operations.
.proc k_encode
 lda #0
 sta k_len
@loop:
 lda k_len
 clc
 adc #3
 cmp k_max
 bcc @get
 beq @get
 clc
 rts
@get:
 lda k_data_pos
 cmp k_data_len
 bne @byte
 lda k_eof
 jne @done
 lda #<k_data
 sta io_ptr
 lda #>k_data
 sta io_ptr+1
 lda #128
 sta io_size
 lda #0
 sta io_size+1
 jsr b_read
 lda io_error
 beq :+
 cmp #136
 bne @bad
 inc k_eof
: lda io_size
 sta k_data_len
 lda #0
 sta k_data_pos
 lda k_data_len
 beq @done
@byte:
 ldx k_data_pos
 lda k_data,x
 inc k_data_pos
 sta k_byte
 ldx k_len
 lda k_qbin
 beq @ctl
 lda k_byte
 bpl @ctl
 lda k_qbin
 sta k_tx+4,x
 inx
 lda k_byte
 and #127
 sta k_byte
@ctl:
 lda k_byte
 and #127
 cmp #32
 bcc @control
 cmp #127
 beq @control
 cmp #'#'
 beq @quote
 cmp k_qbin
 beq @quote
 cmp k_repeat
 bne @raw
@quote:
 lda #'#'
 sta k_tx+4,x
 inx
 jmp @raw
@control:
 lda #'#'
 sta k_tx+4,x
 inx
 lda k_byte
 eor #64
 sta k_byte
@raw:
 lda k_byte
 sta k_tx+4,x
 inx
 stx k_len
 jmp @loop
@done:clc
 rts
@bad:sec
 rts
.endproc
