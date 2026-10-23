; ZMODEM: CRC16/CRC32 receive, hex and binary headers, ZRPOS recovery,
; exact lengths, bounded 1K subpackets, CAN*5 cancellation and batched output.
; No overlap-I/O is advertised: disk commit precedes acknowledgment.
z_hdr: .res 5,0
z_format: .byte 0
z_type: .byte 0
z_end: .byte 0
z_flag: .byte 0
z_index: .byte 0
z_byte: .byte 0
z_nibble: .byte 0
z_count: .word 0
z_outlen: .word 0
z_pos: .res 4,0
z_target: .res 4,0
z_crc: .res 4,0
z_wire: .res 4,0
z_tries: .byte 0
z_open: .byte 0
z_done: .byte 0
z_overflow: .byte 0
z_eof: .byte 0
z_cancel_count: .byte 0
z_limit: .word 1024
z_peer_flags: .byte 0
z_saved_crc: .res 4,0

.proc z_init
 jsr b_begin
 lda #0
 sta z_outlen
 sta z_outlen+1
 sta z_open
 sta z_done
 sta z_eof
 sta z_cancel_count
 ldx #3
@clear:sta z_pos,x
 dex
 bpl @clear
 lda #10
 sta z_tries
 rts
.endproc
.proc z_crc_init
 lda #0
 sta xcrc
 sta xcrc+1
 lda #255
 ldx #3
@fill:sta z_crc,x
 dex
 bpl @fill
 rts
.endproc
.proc z_crc_byte
 ldx z_format
 cpx #$43
 beq @crc32
 eor xcrc+1
 tax
 lda xcrc
 eor z_crc_hi,x
 sta xcrc+1
 lda z_crc_lo,x
 sta xcrc
 rts
@crc32:
 eor z_crc
 tax
 lda z_crc+1
 eor z_crc32_0,x
 sta z_crc
 lda z_crc+2
 eor z_crc32_1,x
 sta z_crc+1
 lda z_crc+3
 eor z_crc32_2,x
 sta z_crc+2
 lda z_crc32_3,x
 sta z_crc+3
 rts
.endproc
.proc z_flush
 lda z_outlen
 ora z_outlen+1
 beq @ok
 lda #<io_buffer
 sta io_ptr
 lda #>io_buffer
 sta io_ptr+1
 lda z_outlen
 sta io_size
 lda z_outlen+1
 sta io_size+1
 jsr b_send
 php
 lda #0
 sta z_outlen
 sta z_outlen+1
 plp
 rts
@ok:clc
 rts
.endproc
.proc z_raw_put
 pha
 lda #<io_buffer
 clc
 adc z_outlen
 sta xptr
 lda #>io_buffer
 adc z_outlen+1
 sta xptr+1
 pla
 ldy #0
 sta (xptr),y
 inc z_outlen
 bne @ok
 inc z_outlen+1
 lda z_outlen+1
 cmp #2
 beq z_flush
@ok:clc
 rts
.endproc
.proc z_put
 sta z_byte
 and #$60
 bne @raw
 lda #24
 jsr z_raw_put
 bcs @out
 lda z_byte
 eor #64
 jmp z_raw_put
@raw:lda z_byte
 jmp z_raw_put
@out:rts
.endproc
.proc z_hex_put
 pha
 lsr
 lsr
 lsr
 lsr
 tax
 lda z_hexchars,x
 jsr z_raw_put
 pla
 and #15
 tax
 lda z_hexchars,x
 jmp z_raw_put
.endproc
z_hexchars:.byte "0123456789abcdef"
; C set on timeout/abort. z_flag=1 denotes a data terminator, else data byte.
.proc z_get
 lda #0
 sta z_flag
@next:jsr b_get
 jcs @bad
 cmp #17
 beq @next
 cmp #19
 beq @next
 cmp #145
 beq @next
 cmp #147
 beq @next
 cmp #24
 beq @escape
 sta z_last
 lda #0
 sta z_cancel_count
 lda z_last
 clc
 rts
@bad:sec
 rts
@escape:
 lda #1
 sta z_cancel_count
@escaped:
 jsr b_get
 bcs @bad
 cmp #24
 bne @code
 inc z_cancel_count
 lda z_cancel_count
 cmp #5
 bcc @escaped
 lda #2
 sta xresult
 bne @bad
@code:
 cmp #17
 beq @escaped
 cmp #19
 beq @escaped
 cmp #145
 beq @escaped
 cmp #147
 beq @escaped
 cmp #$68
 bcc @quoted
 cmp #$6c
 bcs @rub
 inc z_flag
 clc
 rts
@rub:cmp #$6c
 bne :+
 lda #127
 clc
 rts
: cmp #$6d
 bne @quoted
 lda #255
 clc
 rts
@quoted:
 sta z_byte
 and #$60
 cmp #$40
 bne @bad
 lda z_byte
 eor #64
 clc
 rts
.endproc
z_last:.byte 0
.proc z_hex_get
 jsr @digit
 bcs @bad
 asl
 asl
 asl
 asl
 sta z_nibble
 jsr @digit
 bcs @bad
 ora z_nibble
 clc
 rts
@digit:jsr b_get
 bcs @bad
 and #127
 cmp #'0'
 bcc @bad
 cmp #'9'+1
 bcc @decimal
 ora #32
 cmp #'a'
 bcc @bad
 cmp #'f'+1
 bcs @bad
 sec
 sbc #'a'-10
 clc
 rts
@decimal:sec
 sbc #'0'
 clc
 rts
@bad:sec
 rts
.endproc
.proc z_header_byte
 lda z_format
 cmp #$42
 beq z_hex_get
 jsr z_get
 bcs @out
 ldx z_flag
 beq @out
 sec
@out:rts
.endproc
.proc z_header
 lda #0
 sta z_count
 sta z_count+1
@scan:
 jsr b_get
 jcs @bad
 cmp #24
 beq @cancel
 and #127
 cmp #42
 beq @pad
@noise:
 lda #0
 sta z_cancel_count
 inc z_count
 bne @scan
 inc z_count+1
 lda z_count+1
 cmp #16
 bcc @scan
 bcs @bad
@cancel:
 inc z_cancel_count
 lda z_cancel_count
 cmp #5
 bcc @scan
 lda #2
 sta xresult
 bne @bad
@pad:
 lda #0
 sta z_cancel_count
@pads:jsr b_get
 bcs @bad
 and #127
 cmp #42
 beq @pads
 cmp #24
 bne @noise
 jsr b_get
 bcs @bad
 cmp #$41
 bcc @bad
 cmp #$44
 bcs @bad
 sta z_format
 jsr z_crc_init
 lda #0
 sta z_index
@bytes:
 jsr z_header_byte
 bcs @bad
 ldx z_index
 sta z_hdr,x
 jsr z_crc_byte
 inc z_index
 lda z_index
 cmp #5
 bcc @bytes
 jsr z_check_crc
 bcs @bad
 lda z_format
 cmp #$42
 bne :+
 jsr b_get
 bcs @bad
 and #127
 cmp #13
 bne @bad
 jsr b_get
 bcs @bad
 and #127
 cmp #10
 bne @bad
: lda z_hdr
 sta z_type
 clc
 rts
@bad:sec
 rts
.endproc
.proc z_check_crc
 lda z_format
 cmp #$43
 beq @c32
 lda xcrc
 sta z_wire
 lda xcrc+1
 sta z_wire+1
 jsr z_header_byte
 bcs @bad
 cmp z_wire+1
 bne @bad
 jsr z_header_byte
 bcs @bad
 cmp z_wire
 bne @bad
 clc
 rts
@c32:
 lda #0
 sta z_index
@byte:
 jsr z_header_byte
 bcs @bad
 ldx z_index
 eor #255
 cmp z_crc,x
 bne @bad
 inc z_index
 lda z_index
 cmp #4
 bcc @byte
 clc
 rts
@bad:sec
 rts
.endproc
; All outgoing headers use CRC16 hex, accepted by every ZMODEM peer.
.proc z_send_header
 sta z_hdr
 lda #$42
 sta z_format
 jsr z_crc_init
 lda #42
 jsr z_raw_put
 lda #42
 jsr z_raw_put
 lda #24
 jsr z_raw_put
 lda #$42
 jsr z_raw_put
 lda #0
 sta z_index
@byte:
 ldx z_index
 lda z_hdr,x
 pha
 jsr z_crc_byte
 pla
 jsr z_hex_put
 inc z_index
 lda z_index
 cmp #5
 bcc @byte
 lda xcrc+1
 jsr z_hex_put
 lda xcrc
 jsr z_hex_put
 lda #13
 jsr z_raw_put
 lda #10
 jsr z_raw_put
 lda z_hdr
 cmp #8
 beq @flush
 cmp #3
 beq @flush
 lda #17
 jsr z_raw_put
@flush:jmp z_flush
.endproc
.proc z_zero_header
 pha
 lda #0
 ldx #4
@zero:sta z_hdr,x
 dex
 bne @zero
 pla
 jmp z_send_header
.endproc
.proc z_position_header
 pha
 ldx #3
@copy:lda z_pos,x
 sta z_hdr+1,x
 dex
 bpl @copy
 pla
 jmp z_send_header
.endproc
.proc z_position_equal
 ldx #3
@cmp:lda z_hdr+1,x
 cmp z_pos,x
 bne @bad
 dex
 bpl @cmp
 clc
 rts
@bad:sec
 rts
.endproc
.proc z_advance
 lda z_pos
 clc
 adc xlength
 sta z_pos
 lda z_pos+1
 adc xlength+1
 sta z_pos+1
 lda z_pos+2
 adc #0
 sta z_pos+2
 lda z_pos+3
 adc #0
 sta z_pos+3
 rts
.endproc
.proc z_rinit
 lda #0
 sta z_hdr+1
 sta z_hdr+3
 lda #4
 sta z_hdr+2
 lda #$60 ; CANFC32 | ESCCTL; no CANOVIO, CANBRK or streaming promises
 sta z_hdr+4
 lda #1
 jmp z_send_header
.endproc
; Read and verify an entire subpacket before committing any bytes to disk.
.proc z_data_get
 jsr z_crc_init
 lda #0
 sta xlength
 sta xlength+1
 sta z_overflow
@next:
 jsr z_get
 jcs @bad
 ldx z_flag
 bne @end
 sta z_last
 jsr z_crc_byte
 lda xlength+1
 cmp #4
 bcs @overflow
 lda #<xbuf
 clc
 adc xlength
 sta xptr
 lda #>xbuf
 adc xlength+1
 sta xptr+1
 ldy #0
 lda z_last
 sta (xptr),y
 inc xlength
 bne @next
 inc xlength+1
 jmp @next
@overflow:
 inc z_overflow
 bne @next
 jmp @bad
@end:
 sta z_end
 jsr z_crc_byte
 ; Data CRC bytes are escaped, even after a hex header.
 lda z_format
 pha
 cmp #$42
 bne :+
 lda #$41
 sta z_format
: jsr z_check_crc
 php
 pla
 sta z_last
 pla
 sta z_format
 lda z_last
 pha
 plp
 bcs @bad
 lda z_overflow
 bne @bad
 clc
 rts
@bad:sec
 rts
.endproc
.proc z_data_send
 ; A=frame terminator; payload length xlength in xbuf.
 sta z_end
 lda #$41
 sta z_format
 jsr z_crc_init
 lda #0
 sta z_count
 sta z_count+1
@next:
 lda z_count
 cmp xlength
 bne @byte
 lda z_count+1
 cmp xlength+1
 beq @end
@byte:
 lda #<xbuf
 clc
 adc z_count
 sta xptr
 lda #>xbuf
 adc z_count+1
 sta xptr+1
 ldy #0
 lda (xptr),y
 pha
 jsr z_crc_byte
 pla
 jsr z_put
 bcs @bad
 inc z_count
 bne @next
 inc z_count+1
 jmp @next
@end:
 lda #24
 jsr z_raw_put
 lda z_end
 pha
 jsr z_crc_byte
 pla
 jsr z_raw_put
 lda xcrc+1
 jsr z_put
 lda xcrc
 jsr z_put
 lda z_end
 cmp #$6b
 bne :+
 lda #17
 jsr z_raw_put
: jmp z_flush
@bad:sec
 rts
.endproc
.proc z_abort
 lda xresult
 bne :+
 lda #2
 sta xresult
: lda #0
 sta z_outlen
 sta z_outlen+1
 lda #8
 sta z_index
@cancel:
 lda #24
 jsr z_raw_put
 dec z_index
 bne @cancel
 jsr z_flush
 lda z_open
 beq @out
 jsr b_close
@out:rts
.endproc
.proc z_disk_error
 lda #4
 sta xresult
 jmp z_abort
.endproc
.proc z_retry
 lda xresult
 bne @bad
 dec z_tries
 beq @timeout
 clc
 rts
@timeout:lda #1
 sta xresult
@bad:sec
 rts
.endproc
.proc z_receive
 jsr z_init
 jsr z_rinit
@header:
 jsr z_header
 jcs @retry
 lda z_type
 cmp #0
 beq @init
 cmp #2
 jeq @sinit
 cmp #4
 beq @file
 cmp #10
 jeq @data
 cmp #11
 jeq @eof
 cmp #8
 jeq @finish
 cmp #14
 jeq @challenge
 cmp #7
 jeq z_abort
 cmp #16
 jeq z_abort
 ; Unknown/unsupported command cannot execute anything on this computer.
 lda #6
 jsr z_zero_header
 jmp @retry
@init:jsr z_rinit
 jmp @header
@file:
 jsr z_data_get
 jcs @retry
 lda z_done
 beq :+
 ; The UI names one local file; skip additional batch members explicitly.
 lda #5
 jsr z_zero_header
 jmp @header
: lda z_open
 bne @position
 jsr b_binary
 jcs z_abort
 jsr z_receive_open
 lda xresult
 jne z_abort
 lda io_error
 jne z_disk_error
 lda #1
 sta z_open
 lda #10
 sta z_tries
@position:
 lda #9
 jsr z_position_header
 jmp @header
@data:
 lda z_open
 jeq @retry
 jsr z_position_equal
 jcs @position
@subpacket:
 jsr z_data_get
 jcs @retry
 lda #<xbuf
 sta io_ptr
 lda #>xbuf
 sta io_ptr+1
 lda xlength
 sta io_size
 lda xlength+1
 sta io_size+1
 ora io_size
 beq @committed
 jsr b_write
 lda io_error
 jne z_disk_error
@committed:
 jsr z_advance
 lda #10
 sta z_tries
 lda z_end
 cmp #$69 ; ZCRCG, continue without ACK
 beq @subpacket
 cmp #$6a ; ZCRCQ, continue after ACK
 beq @ackmore
 cmp #$6b ; ZCRCW, ACK then wait for next header
 jne @header
 lda #3
 jsr z_position_header
 jmp @header
@ackmore:
 lda z_format
 pha
 lda #3
 jsr z_position_header
 pla
 sta z_format
 jmp @subpacket
@eof:
 jsr z_position_equal
 jcs @position
 lda z_open
 jeq @init
 jsr b_close
 lda io_error
 jne z_disk_error
 lda #0
 sta z_open
 lda #1
 sta z_done
 jmp @init
@sinit:
 jsr z_data_get
 jcs @retry
 lda #3
 jsr z_zero_header
 jmp @header
@challenge:
 lda #3
 jsr z_send_header
 jmp @header
@finish:
 lda z_open
 jne z_abort
 lda #8
 jsr z_zero_header
 ; Consume sender's OO so it does not leak into terminal text.
@oo: jsr b_get
 bcs @ok
 cmp #17
 beq @oo
 cmp #'O'
 bne @ok
 jsr b_get
@ok:lda #0
 sta xresult
 rts
@retry:
 jsr z_retry
 jcs z_abort
 lda z_open
 jne @position
 jsr z_rinit
 jmp @header
.endproc
; Resume is explicitly selected by the user. Verify the entire existing
; prefix against the sender's ZCRC response before opening it for append.
.proc z_receive_open
 lda z_resume_requested
 jeq @fresh
 lda #4
 sta io_mode
 jsr b_open
 lda io_error
 beq @measure
 cmp #170
 jeq @fresh
 rts
@measure:
 lda #0
 sta z_limit
 lda #4
 sta z_limit+1
 lda #$43
 sta z_format
 jsr z_crc_init
@read:
 jsr z_read
 jcs @failed
 lda #0
 sta z_count
 sta z_count+1
@crc:
 lda z_count
 cmp xlength
 bne @byte
 lda z_count+1
 cmp xlength+1
 beq @advance
@byte:
 lda #<xbuf
 clc
 adc z_count
 sta xptr
 lda #>xbuf
 adc z_count+1
 sta xptr+1
 ldy #0
 lda (xptr),y
 jsr z_crc_byte
 inc z_count
 bne @crc
 inc z_count+1
 jmp @crc
@advance:
 jsr z_advance
 lda z_eof
 beq @read
 ldx #3
@save:lda z_crc,x
 eor #255
 sta z_saved_crc,x
 dex
 bpl @save
 jsr b_close
 lda z_pos
 ora z_pos+1
 ora z_pos+2
 ora z_pos+3
 beq @append
 lda #13
 jsr z_position_header
@reply:
 jsr z_header
 bcs @retry
 lda z_type
 cmp #13
 bne @retry
 ldx #3
@compare:lda z_hdr+1,x
 cmp z_saved_crc,x
 bne @mismatch
 dex
 bpl @compare
@append:
 lda #9
 sta io_mode
 jmp b_open
@retry:
 jsr z_retry
 bcs @mismatch
 lda #13
 jsr z_position_header
 jmp @reply
@mismatch:
 lda #7
 sta xresult
 lda #144
 sta io_error
 rts
@failed:
 jsr b_close
 lda #144
 sta io_error
 rts
@fresh:
 lda #8
 sta io_mode
 jmp b_open
.endproc
.proc z_read
 lda #<xbuf
 sta io_ptr
 lda #>xbuf
 sta io_ptr+1
 lda z_limit
 sta io_size
 lda z_limit+1
 sta io_size+1
 jsr b_read
 lda io_size
 sta xlength
 lda io_size+1
 sta xlength+1
 lda io_error
 beq @ok
 cmp #136
 bne @bad
 lda #1
 sta z_eof
@ok:clc
 rts
@bad:sec
 rts
.endproc
; Portable recovery seek: reopen and discard to the requested byte offset.
; It is used on recovery only; normal data is read once.
.proc z_seek
 ldx #3
@save:lda z_hdr+1,x
 sta z_target,x
 lda #0
 sta z_pos,x
 dex
 bpl @save
 jsr b_close
 lda #4
 sta io_mode
 jsr b_open
 lda io_error
 jne @bad
 lda #0
 sta z_eof
@loop:
 ldx #3
@equal:lda z_target,x
 cmp z_pos,x
 bne @read
 dex
 bpl @equal
 clc
 rts
@read:
 lda z_target
 sec
 sbc z_pos
 sta z_limit
 lda z_target+1
 sbc z_pos+1
 sta z_limit+1
 lda z_target+2
 sbc z_pos+2
 sta z_last
 lda z_target+3
 sbc z_pos+3
 ora z_last
 bne @full
 lda z_limit+1
 cmp #4
 bcc @part
@full:lda #0
 sta z_limit
 lda #4
 sta z_limit+1
@part:jsr z_read
 bcs @bad
 jsr z_advance
 lda z_eof
 beq @loop
 ; EOF exactly at the requested position is valid (zero bytes remain).
 ldx #3
@check:lda z_pos,x
 cmp z_target,x
 bne @bad
 dex
 bpl @check
 clc
 rts
@bad:sec
 rts
.endproc
.proc z_filename
 lda #0
 ldx #127
@zero:sta xbuf,x
 dex
 bpl @zero
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
 beq @done
 sta xbuf,y
 inx
 iny
 cpy #63
 bcc @copy
@done:iny
 ; File size is optional in ZMODEM. ZEOF carries the exact final position.
 sty xlength
 lda #0
 sta xlength+1
 rts
.endproc
.proc z_send
 jsr z_init
 lda #4
 sta io_mode
 jsr b_open
 lda io_error
 jne z_disk_error
 lda #1
 sta z_open
 lda #0
 jsr z_zero_header
@init:
 jsr z_header
 bcs @initretry
 lda z_type
 cmp #1
 beq @file
 cmp #8
 jeq @finish
 cmp #7
 jeq z_abort
@initretry:
 jsr z_retry
 jcs z_abort
 lda #0
 jsr z_zero_header
 jmp @init
@file:
 jsr b_binary
 jcs z_abort
 lda z_hdr+4
 sta z_peer_flags
 ; Send conservatively in 1K acknowledged frames. Respect smaller buffers.
 lda z_hdr+1
 sta z_limit
 lda z_hdr+2
 sta z_limit+1
 ora z_limit
 beq @default
 lda z_limit+1
 cmp #4
 bcc @metadata
@default:lda #0
 sta z_limit
 lda #4
 sta z_limit+1
@metadata:
 lda #0
 sta z_hdr+1
 sta z_hdr+2
 sta z_hdr+3
 lda #1 ; ZCBIN
 sta z_hdr+4
 lda #4
 jsr z_send_header
 jsr z_filename
 lda #$6b
 jsr z_data_send
@position:
 jsr z_header
 bcs @metaretry
 lda z_type
 cmp #1
 beq @position
 cmp #9
 beq @seek
 cmp #5
 jeq @finish
 cmp #7
 jeq z_abort
@metaretry:
 jsr z_retry
 jcs z_abort
 jmp @metadata
@seek:
 ; Preserve peer's max packet length across the discard/seek loop.
 lda z_limit
 pha
 lda z_limit+1
 pha
 jsr z_seek
 php
 pla
 sta z_last
 pla
 sta z_limit+1
 pla
 sta z_limit
 lda z_last
 pha
 plp
 jcs z_disk_error
 lda #10
 sta z_tries
@read:
 jsr z_read
 jcs z_disk_error
 lda xlength
 ora xlength+1
 jeq @eof
 lda #10
 jsr z_position_header
 lda #$6b
 jsr z_data_send
 jcs z_abort
 jsr z_advance
@ack:
 jsr z_header
 bcs @ackretry
 lda z_type
 cmp #9
 jeq @seek
 cmp #3
 bne @ackretry
 jsr z_position_equal
 bcs @ackretry
 lda #10
 sta z_tries
 jmp @read
@ackretry:
 jsr z_retry
 jcs z_abort
 ; Re-send from the start of the retained packet on timeout.
 lda z_pos
 sec
 sbc xlength
 sta z_pos
 lda z_pos+1
 sbc xlength+1
 sta z_pos+1
 lda z_pos+2
 sbc #0
 sta z_pos+2
 lda z_pos+3
 sbc #0
 sta z_pos+3
 lda #10
 jsr z_position_header
 lda #$6b
 jsr z_data_send
 jsr z_advance
 jmp @ack
@eof:
 lda #11
 jsr z_position_header
 jsr z_header
 bcs @eofretry
 lda z_type
 cmp #9
 jeq @seek
 cmp #1
 beq @finish
@eofretry:
 jsr z_retry
 jcs z_abort
 jmp @eof
@finish:
 lda #8
 jsr z_zero_header
 jsr z_header
 bcs @finretry
 lda z_type
 cmp #8
 bne @finretry
 lda #'O'
 jsr z_raw_put
 lda #'O'
 jsr z_raw_put
 jsr z_flush
 jsr b_close
 lda io_error
 jne z_disk_error
 lda #0
 sta z_open
 sta xresult
 rts
@finretry:
 jsr z_retry
 jcs z_abort
 jmp @finish
.endproc
.include "z_crc_tables.inc"
