.setcpu "6502"
.feature labels_without_colons
.include "cart-symbols.inc"
.segment "BOOT"
.org $8000
; Type 63 powers up in bank 254. The init vector only returns; the run
; vector copies the gateway below both cartridge windows before banking.
boot_start:
 sei
 cld
 ldx #$ff
 txs
 lda #0
 sta $d40e
 sta VBXE_BASE+$5f
 sta VBXE_BASE+$5d
 lda $d301
 ora #2
 sta $d301
 ldx #0
@copy:
 lda $9000,x
 sta $2500,x
 lda $9100,x
 sta $2600,x
 lda $9200,x
 sta $2700,x
 inx
 bne @copy
 lda #254
 sta cart_return_bank
 lda #0
 sta cart_nmi
 ; Close previous channels on a restart. Native E: remains the OS console.
 ldx #$10
@close:
 lda #12
 sta $0342,x
 jsr $e456
 txa
 clc
 adc #16
 tax
 bpl @close
 ; Remove only our R: entry before reinstalling its original relocator.
 ldx #0
@r:
 lda $031a,x
 cmp #'R'
 bne :+
 lda #0
 sta $031a,x
 sta $031b,x
 sta $031c,x
: inx
 inx
 inx
 cpx #$24
 bcc @r
 lda #<dos_table
 ldx #>dos_table
 jsr boot_load_table
 lda #$e0
 sta $0c
 lda #7
 sta $0d
 ; MyDOS warm initialization registers D: and its three buffers. Bit 5
 ; suppresses disk autoruns: all program resources now come from ROM.
 lda $07bb
 ora #$20
 sta $07bb
 jsr $07e0
 lda #<serial_table
 ldx #>serial_table
 jsr boot_load_table
 jsr $3502
 ; The supplied MyDOS/R: pair must end below the gateway at $2500.
 lda $02e8
 cmp #$25
 bcc @memory_ok
 bne boot_fail
 lda $02e7
 bne boot_fail
@memory_ok:
 lda #<cart_reboot
 sta $0a
 sta $0c
 lda #>cart_reboot
 sta $0b
 sta $0d
 lda #1
 sta $09
 lda #0
 sta $d40e
 sta $022f
 sta $d400
 lda #<program_table
 ldx #>program_table
 jsr boot_load_table
 jmp cart_launch
boot_fail:
 lda #$34
 sta $d01a
 jmp boot_fail

boot_load_table:
 sta $f8
 stx $f9
@entry:
 ldy #0
@src: lda ($f8),y
 sta cart_src,y
 iny
 cpy #3
 bne @src
@args: lda ($f8),y
 sta cart_dest-3,y
 iny
 cpy #7
 bne @args
 lda cart_len
 ora cart_len+1
 beq @end
 jsr cart_read
 clc
 lda $f8
 adc #7
 sta $f8
 bcc @entry
 inc $f9
 bne @entry
@end:rts
boot_init:
 rts
.include "cart-assets.inc"
.assert * <= $9000, error, "Boot code overlaps low-RAM gateway copy"
.res $9000-*, $ff
.incbin "cart-runtime.bin"
.res $bffa-*, $ff
.word boot_start
.byte 0,4
.word boot_init
