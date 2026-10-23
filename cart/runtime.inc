; Flash MegaCart type 63. Only this low-RAM code executes with ROM mapped.
; Four-byte bursts bound the IRQ/NMI blackout and never overlap VBXE MEMAC A.
; The terminal uses OS VBI only (NMIEN=$40); cart_nmi is zero during boot.
.export _cart_font, _cart_palette
.segment "CART"
cart_runtime_start:
cart_src: .res 3,0
cart_dest: .word 0
cart_len: .word 0
cart_return_bank: .byte 255
cart_nmi: .byte $40
cart_bank: .byte 0
cart_saved_memac: .byte 0
cart_count: .byte 0
cart_pages: .byte 0
cart_vbank: .byte 0
cart_mul: .res 3,0
cart_counter: .byte 0
cart_buffer: .res 4,0

.proc cart_read
 lda cart_src
 sta @get+1
 lda cart_src+1
 and #$3f
 ora #$80
 sta @get+2
 lda cart_src+2
 asl
 asl
 sta cart_bank
 lda cart_src+1
 rol
 rol
 rol
 and #3
 ora cart_bank
 sta cart_bank
@burst:
 lda cart_len
 ora cart_len+1
 jeq @done
 php
 sei
 lda #0
 sta $d40e
 lda memac_bank_sel
 sta cart_saved_memac
 lda #0
 sta memac_bank_sel
 lda cart_bank
 sta $d500
 ldx #0
@get: lda $8000
 sta cart_buffer,x
 inc @get+1
 bne @advance
 inc @get+2
 lda @get+2
 cmp #$c0
 bne @advance
 lda #$80
 sta @get+2
 inc cart_bank
 lda cart_bank
 sta $d500
@advance:
 inc cart_src
 bne :+
 inc cart_src+1
 bne :+
 inc cart_src+2
:
 lda cart_len
 bne :+
 dec cart_len+1
: dec cart_len
 inx
 lda cart_len
 ora cart_len+1
 beq @unmap
 cpx #4
 bne @get
@unmap:
 stx cart_count
 lda #255
 sta $d500
 lda cart_saved_memac
 sta memac_bank_sel
 lda cart_nmi
 sta $d40e
 plp
 lda cart_dest
 sta @put+1
 lda cart_dest+1
 sta @put+2
 ldy #0
@copy: lda cart_buffer,y
@put: sta $ffff,y
 iny
 cpy cart_count
 bne @copy
 tya
 clc
 adc cart_dest
 sta cart_dest
 bcc :+
 inc cart_dest+1
: jmp @burst
@done:
 lda cart_return_bank
 sta $d500
 rts
.endproc

; A=ROM page within $10000..$1ffff, X=VBXE 4K bank, Y=number of banks.
.proc cart_load_banks
 sta cart_src+1
 stx cart_vbank
 sty cart_pages
 lda #1
 sta cart_src+2
 lda #0
 sta cart_src
 lda memac_bank_sel
 pha
@page:
 lda cart_vbank
 sta memac_bank_sel
 lda #0
 sta cart_dest
 sta cart_len
 lda #$a0
 sta cart_dest+1
 lda #16
 sta cart_len+1
 jsr cart_read
 inc cart_vbank
 dec cart_pages
 bne @page
 pla
 sta memac_bank_sel
 rts
.endproc

.proc _cart_font
 lda memac_bank_sel
 pha
 lda #$80
 sta memac_bank_sel
 lda active_font
 sec
 sbc #3
 asl
 asl
 asl
 clc
 adc #$80
 sta cart_src+1
 lda #1
 sta cart_src+2
 lda #0
 sta cart_src
 sta cart_dest
 sta cart_len
 lda #$a0
 sta cart_dest+1
 lda #8
 sta cart_len+1
 jsr cart_read
 pla
 sta memac_bank_sel
 ldx #0
 lda #1
 rts
.endproc

.proc _cart_palette
 lda #0
 sta cart_src
 sta cart_dest
 sta cart_len+1
 lda #$e8
 sta cart_src+1
 lda #1
 sta cart_src+2
 lda #$a8
 sta cart_dest+1
 lda #48
 sta cart_len
 jmp cart_read
.endproc

; Re-enter the boot ROM on RESET or the cartridge's Restart menu command.
cart_reboot:
 sei
 lda #0
 sta $d40e
 sta memac_bank_sel
 sta memac_b_control
 lda #254
 sta $d500
 jmp $8000
cart_launch:
 lda #255
 sta cart_return_bank
 sta $d500
 lda #$40
 sta cart_nmi
 sta $d40e
 cli
 jmp start
cart_runtime_end:
.assert cart_runtime_end <= $2800, lderror, "Cartridge gateway overlaps program"

; Reconstruct exactly the original UFN1 sector from 17-byte glyph records.
; The removed 15 bytes in each 32-byte disk record must all be zero.
.segment "RESIDENT"
.proc cart_unicode_read
 lda u_sector_no
 ora u_sector_no+1
 jeq @bad
 lda u_sector_no+1
 cmp #>(CART_FONT_SECTORS+1)
 bcc @valid
 jne @bad
 lda u_sector_no
 cmp #<(CART_FONT_SECTORS+1)
 jcs @bad
@valid:
 lda #<u_sector
 sta cart_dest
 lda #>u_sector
 sta cart_dest+1
 lda #0
 sta cart_len
 lda #1
 sta cart_len+1
 lda u_sector_no+1
 bne @glyphs
 lda u_sector_no
 cmp #38
 bcs @glyphs
 cmp #4
 bcs @directory
 sec
 sbc #1
 lsr
 sta cart_src+1
 lda #0
 ror
 sta cart_src
 lda #128
 sta cart_len
 lda #0
 sta cart_len+1
 beq @header
@directory:
 sec
 sbc #3
 sta cart_src+1
 lda #128
 sta cart_src
@header:
 lda #2
 sta cart_src+2
 jsr cart_read
 jmp @good
@glyphs:
 sec
 lda u_sector_no
 sbc #38
 sta cart_src
 lda u_sector_no+1
 sbc #0
 sta cart_src+1
 lda #0
 sta cart_src+2
 ldx #3
 jsr @shift
 ldx #2
@save: lda cart_src,x
 sta cart_mul,x
 dex
 bpl @save
 ldx #4
 jsr @shift
 clc
 lda cart_src
 adc cart_mul
 sta cart_src
 lda cart_src+1
 adc cart_mul+1
 sta cart_src+1
 lda cart_src+2
 adc cart_mul+2
 sta cart_src+2
 clc
 lda cart_src+1
 adc #$24
 sta cart_src+1
 lda cart_src+2
 adc #2
 sta cart_src+2
 lda #136
 sta cart_len
 lda #0
 sta cart_len+1
 jsr cart_read
 ldx #135
 ldy #255
@record:
 lda #15
 sta cart_counter
 lda #0
@zero: sta u_sector,y
 dey
 dec cart_counter
 bne @zero
 lda #17
 sta cart_counter
@copy: lda u_sector,x
 sta u_sector,y
 dex
 dey
 dec cart_counter
 bne @copy
 cpy #255
 bne @record
@good:
 lda u_sector_no
 sta u_cached_sector
 lda u_sector_no+1
 sta u_cached_sector+1
 lda #1
 sta u_disk_error
 rts
@bad:
 lda #144
 sta u_disk_error
 rts
@shift:
 asl cart_src
 rol cart_src+1
 rol cart_src+2
 dex
 bne @shift
 rts
.endproc
.segment "CODE"
