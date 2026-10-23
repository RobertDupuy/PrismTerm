; Telnet framing for FujiNet TCP sockets. R: and raw TCP remain byte-transparent.
; RFC 854/856, NAWS (1073), terminal type (1091). Only supported options accepted.
tn_ptr=$cc
.segment "BSS"
telnet_on: .res 1
tn_state: .res 1
tn_command: .res 1
tn_option: .res 1
tn_bit: .res 1
tn_side: .res 1
tn_enabled: .res 2
tn_pending: .res 2
tn_subopt: .res 1
tn_request: .res 1
tn_cr: .res 1
tn_value: .res 1
tn_remaining: .res 2
tn_chunk: .res 1
tn_reply: .res 16
tn_buffer: .res 256
.segment "CODE"
.proc telnet_start
 lda #0
 sta telnet_on
 sta tn_state
 sta tn_cr
 sta tn_enabled
 sta tn_enabled+1
 sta tn_pending
 sta tn_pending+1
 lda device_type
 beq @out
 lda proto_byte
 cmp #'T'
 bne @out
 lda #1
 sta telnet_on
 sta tn_pending
 sta tn_pending+1
 lda #<tn_offer
 ldx #>tn_offer
 ldy #6
 jmp tn_send_string
@out: rts
.endproc
tn_offer: .byte 255,251,0,255,253,0
tn_type: .byte 255,250,24,0,"VT100",255,240
.proc tn_send_string
 sta io_ptr
 stx io_ptr+1
 sty io_size
 lda #0
 sta io_size+1
 jmp wire_raw_send
.endproc
.proc tn_send_reply
 lda #<tn_reply
 ldx #>tn_reply
 jmp tn_send_string
.endproc
.proc plus_network_byte
 lda telnet_on
 beq @data
 lda temp_char
 jsr telnet_filter
 bcs @out
 sta temp_char
@data: jsr plus_capture_byte
 jmp process_char
@out: rts
.endproc
; A=input. C=1 means protocol byte consumed; C=0 returns a data byte.
.proc telnet_filter
 sta tn_value
 lda tn_state
 beq @normal
 cmp #1
 jeq @iac
 cmp #2
 jeq @option
 cmp #3
 jeq @sub
 cmp #4
 jeq @sub_iac
@normal: lda tn_value
 cmp #255
 beq @start
 lda tn_enabled
 and #1
 bne @binary
 lda tn_cr
 beq :+
 lda #0
 sta tn_cr
 lda tn_value
 beq @ignore
: lda tn_value
 cmp #13
 bne @binary
 lda #1
 sta tn_cr
@binary: lda tn_value
 clc
 rts
@start: lda #1
 sta tn_state
@ignore: sec
 rts
@iac: lda #0
 sta tn_state
 lda tn_value
 cmp #255
 beq @binary
 cmp #250
 beq @sub_start
 cmp #251
 bcc @ignore
 cmp #255
 bcs @ignore
 sta tn_command
 lda #2
 sta tn_state
 bne @ignore
@sub_start:
 lda #3
 sta tn_state
 lda #255
 sta tn_subopt
 lda #0
 sta tn_request
 beq @ignore
@option: lda #0
 sta tn_state
 jsr tn_negotiate
 sec
 rts
@sub: lda tn_value
 cmp #255
 beq @sub_escape
 ldx tn_subopt
 cpx #255
 bne :+
 sta tn_subopt
 jmp @ignore
: cpx #24
 bne @ignore
 cmp #1
 bne @ignore
 sta tn_request
 jmp @ignore
@sub_escape: lda #4
 sta tn_state
 bne @ignore
@sub_iac: lda #3
 sta tn_state
 lda tn_value
 cmp #240
 bne @ignore
 lda #0
 sta tn_state
 lda tn_subopt
 cmp #24
 bne @ignore
 lda tn_request
 jeq @ignore
 lda tn_enabled+1
 and #8
 jeq @ignore
 lda #<tn_type
 ldx #>tn_type
 ldy #11
 jsr tn_send_string
 sec
 rts
.endproc
tn_options: .byte 0,1,3,24,31
tn_bits: .byte 1,2,4,8,16
tn_allowed: .byte 7,29
tn_yes: .byte 253,251
tn_no: .byte 254,252
.proc tn_negotiate
 lda tn_value
 sta tn_option
 sta tn_reply+2
 ldx #4
@find: cmp tn_options,x
 beq @found
 dex
 bpl @find
 lda #0
 beq @bit
@found: lda tn_bits,x
@bit: sta tn_bit
 ldx #0
 lda tn_command
 cmp #253
 bcc :+
 inx
: stx tn_side
 lda #255
 sta tn_reply
 lda tn_command
 and #1
 beq @disable
 lda tn_bit
 and tn_allowed,x
 beq @refuse
 lda tn_enabled,x
 and tn_bit
 bne @out
 lda tn_enabled,x
 ora tn_bit
 sta tn_enabled,x
 lda tn_pending,x
 and tn_bit
 bne @pending
 lda tn_yes,x
 sta tn_reply+1
 ldy #3
 jsr tn_send_reply
 jmp @naws
@pending: lda tn_bit
 eor #255
 and tn_pending,x
 sta tn_pending,x
@naws: lda tn_option
 cmp #31
 bne @out
 jmp telnet_naws
@disable: lda tn_enabled,x
 and tn_bit
 pha
 lda tn_bit
 eor #255
 and tn_pending,x
 sta tn_pending,x
 lda tn_bit
 eor #255
 and tn_enabled,x
 sta tn_enabled,x
 pla
 beq @out
@refuse: lda tn_no,x
 sta tn_reply+1
 ldy #3
 jmp tn_send_reply
@out: rts
.endproc
.proc telnet_naws
 lda telnet_on
 beq @out
 lda tn_enabled+1
 and #16
 beq @out
 lda #255
 sta tn_reply
 sta tn_reply+7
 lda #250
 sta tn_reply+1
 lda #31
 sta tn_reply+2
 lda #0
 sta tn_reply+3
 sta tn_reply+5
 lda term_last_col
 clc
 adc #1
 sta tn_reply+4
 lda #24
 sta tn_reply+6
 lda #240
 sta tn_reply+8
 ldy #9
 jmp tn_send_reply
@out: rts
.endproc
.proc wire_get
@again: jsr wire_raw_get
 bcs @out
 ldx telnet_on
 beq @out
 jsr telnet_filter
 bcs @again
@out: rts
.endproc
; Escape IAC, and encode NVT CR when binary has not been negotiated.
; Packs at most 128 input bytes into each 256-byte SIO write.
.proc wire_send
 lda telnet_on
 bne @telnet
 jmp wire_raw_send
@telnet: lda io_ptr
 sta tn_ptr
 lda io_ptr+1
 sta tn_ptr+1
 lda io_size
 sta tn_remaining
 lda io_size+1
 sta tn_remaining+1
@chunk: lda tn_remaining
 ora tn_remaining+1
 beq @done
 lda #128
 sta tn_chunk
 ldx #0
@pack: ldy #0
 lda (tn_ptr),y
 sta tn_buffer,x
 inx
 cmp #255
 beq @double
 cmp #13
 bne @next
 lda tn_enabled+1
 and #1
 bne @next
 lda #0
 beq @double
@double: sta tn_buffer,x
 inx
@next: inc tn_ptr
 bne :+
 inc tn_ptr+1
: lda tn_remaining
 bne :+
 dec tn_remaining+1
: dec tn_remaining
 lda tn_remaining
 ora tn_remaining+1
 beq @send
 dec tn_chunk
 bne @pack
@send: stx io_size
 lda #0
 sta io_size+1
 cpx #0
 bne :+
 inc io_size+1
: lda #<tn_buffer
 sta io_ptr
 lda #>tn_buffer
 sta io_ptr+1
 jsr wire_raw_send
 bcs @out
 jmp @chunk
@done: clc
@out: rts
.endproc
