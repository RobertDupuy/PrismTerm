; Resident gateways. MEMAC B is enabled only inside the protocol engine.
; Every OS/transport call sees ordinary Atari RAM, including any R: driver.
.segment "BSS"
proto_ready: .res 1
proto_saved_key: .res 2
proto_saved_proceed: .res 2
proto_entry: .res 2
proto_bank: .res 1
proto_depth: .res 1
proto_flags: .res 1
z_resume_requested: .res 1
.segment "RESIDENT"
.proc proto_load
 lda #0
 sta proto_ready
 lda #<proto_path
 ldx #>proto_path
 jsr plus_set_filename
 lda #4
 sta io_mode
 jsr plus_file_open
 lda io_error
 bne @out
 lda memac_bank_sel
 pha
 lda #$84
 sta memac_bank_sel
@page:
 lda #0
 sta io_ptr
 sta io_size
 lda #$a0
 sta io_ptr+1
 lda #16
 sta io_size+1
 jsr plus_file_read
 lda io_error
 bne @close
 inc memac_bank_sel
 lda memac_bank_sel
 cmp #$88
 bne @page
 inc proto_ready
@close:
 pla
 sta memac_bank_sel
 jsr plus_file_close
@out:rts
.endproc
proto_path: .byte "D:TRANS.BIN",$9b

.macro gate name,target
.proc name
 php
 sei
 pha
 txa
 pha
 lda proto_bank
 pha
 lda #0
 sta proto_bank
 sta memac_b_control
 tsx
 lda $0104,x
 pha
 lda $0103,x
 pha
 lda $0102,x
 tax
 pla
 plp
 jsr target
 php
 sei
 sta proto_gate_a
 stx proto_gate_x
 pla
 sta proto_gate_p
 pla
 sta proto_bank
 sta memac_b_control
 pla
 pla
 pla
 lda proto_gate_p
 pha
 ldx proto_gate_x
 lda proto_gate_a
 plp
 rts
.endproc
.endmacro
proto_gate_a: .byte 0
proto_gate_x: .byte 0
proto_gate_p: .byte 0
gate b_get,wire_get
gate b_send,wire_send
gate b_open,plus_file_open
gate b_close,plus_file_close
gate b_read,plus_file_read
gate b_write,plus_file_write
gate b_begin,xfer_begin
gate b_binary,xfer_binary_ready
gate b_blit,plus_blit
gate b_blit_reset,plus_blit_reset
gate b_udisk,u_disk_read
gate b_scroll_page,scroll_page

.proc proto_enter
 sta proto_entry
 stx proto_entry+1
 lda proto_ready
 bne :+
 lda #6
 sta xresult
 rts
: lda #$81
 jmp bank_enter
.endproc
; A selects the VBXE 16K bank. Entry pointer was set by the caller.
; Nesting is needed when the UI invokes Unicode rendering or a protocol.
.proc bank_enter
 sta proto_target
 lda proto_bank
 pha
 php
 sei
 lda proto_depth
 bne @vectors
 lda VKEYBD
 sta proto_saved_key
 lda VKEYBD+1
 sta proto_saved_key+1
 lda VPRCED
 sta proto_saved_proceed
 lda VPRCED+1
 sta proto_saved_proceed+1
 lda #<proto_key_irq
 sta VKEYBD
 lda #>proto_key_irq
 sta VKEYBD+1
 lda #<proto_proceed_irq
 sta VPRCED
 lda #>proto_proceed_irq
 sta VPRCED+1
@vectors:
 inc proto_depth
 lda proto_target
 sta proto_bank
 sta memac_b_control
 plp
 jsr @run
 php
 sei
 dec proto_depth
 bne @restore_bank
 lda proto_saved_key
 sta VKEYBD
 lda proto_saved_key+1
 sta VKEYBD+1
 lda proto_saved_proceed
 sta VPRCED
 lda proto_saved_proceed+1
 sta VPRCED+1
@restore_bank:
 pla
 sta proto_flags
 pla
 sta proto_bank
 sta memac_b_control
 lda proto_flags
 pha
 plp
 rts
@run:jmp (proto_entry)
.endproc
proto_target: .byte 0
; The OS has pushed A before jumping through either vector. Supply a
; synthetic RTI frame to the original handler, then restore the bank and
; return through the original IRQ frame. X/Y follow the original handler ABI.
.macro bank_irq name,vector
.proc name
 lda proto_bank
 pha
 lda #>@resume
 pha
 lda #<@resume
 pha
 php
 lda #0
 pha
 sta proto_bank
 sta memac_b_control
 jmp (vector)
@resume:
 pla
 sta proto_bank
 sta memac_b_control
 pla
 rti
.endproc
.endmacro
bank_irq proto_key_irq,proto_saved_key
bank_irq proto_proceed_irq,proto_saved_proceed
.proc proto_reset
 lda #0
 sta proto_bank
 sta memac_b_control
 jmp reset_cleanup
.endproc
zmodem_receive:
 lda #0
 sta z_resume_requested
 lda xmode
 cmp #4
 bne :+
 inc z_resume_requested
: lda #<z_receive
 ldx #>z_receive
 jmp proto_enter
zmodem_send:
 lda #<z_send
 ldx #>z_send
 jmp proto_enter
kermit_receive:
 lda #<k_receive
 ldx #>k_receive
 jmp proto_enter
kermit_send:
 lda #<k_send
 ldx #>k_send
 jmp proto_enter
.segment "CODE"
