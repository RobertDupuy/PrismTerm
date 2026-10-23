.ifndef VBXE_BASE
VBXE_BASE = $d600
.endif
;###################################################################################################################
;
;
;	   ##	 #######   ##	 #####	  ###		 ######	  ####	 #    #	   ##	 ####### ######	  ####
;	  #  #	 #  #  #  #  #	  #   #	   #		  #   #	 #    #	 #    #	  #  #	 #  #  #  #   #	 #    #
;	 #    #	    #	 #    #   #   #	   #		  # #	 #    #	 #    #	 #    #	    #	  # #	 #
;	 #    #	    #	 #    #	  ####	   #		  ###	 #    #	 #    #	 #    #	    #	  ###	  ####
;	 ######	    #	 ######   # #	   #		  # #	 #  # #	 #    #	 ######	    #	  # #	      #
;	 #    #	    #	 #    #	  #  #	   #		  #   #	  ####	 #    #	 #    #	    #	  #   #	 #    #
;	 #    #	   ###	 #    #	 ###  #	  ###		 ######	      ##  ####	 #    #	   ###	 ######	  ####
;
;	VBXE Equates
;###################################################################################################################
; VBXE equates
core_version	=	(VBXE_BASE+$40)			; tells us whether FX or GTIA-emu core (READ-only)

; XDL address registers (3 consecutive bytes for 24-bit address)
xdl_adr		=	(VBXE_BASE+$41)			; XDL address low byte
xdl_adr_mid	=	(VBXE_BASE+$42)			; XDL address middle byte  
xdl_adr_high	=	(VBXE_BASE+$43)			; XDL address high byte

; memory window B control register
; bit 0 - 4 - bank number (banks are all 16K)
; bit 6 - MBAE - ANTIC access enable
; bit 7 - MBCE - CPU access enable

memac_b_control	=	(VBXE_BASE+$5D)

; memory window A control register
; bit 0 and 1 - window size - 00 for 4K, 01 for 8K, 10 for 16K, 11 for 32K
; bit 2 - MAE - when set, antic can see VBXE RAM
; bit 3 - MCE - when set, CPU can see VBXE RAM

memac_control	=	(VBXE_BASE+$5E)

memac_bank_sel	=	(VBXE_BASE+$5F)			; controls bank number of movable window (MEMAC A). bit 7 is an enable bit
; Palette write port.  Register order here is CSEL then PSEL — see the FX core register
; map, "Dx44 CSEL / Dx45 PSEL".  They were transposed in this file until v0.22; the bug
; hid because every read of Dx40-Dx5C returns 255, so the old `inc csel` was really an
; `inc` of PSEL that read $FF and stored $00, pinning PSEL to palette 0 while CB's own
; auto-increment did the colour stepping that `inc csel` appeared to be doing.
csel		=	(VBXE_BASE+$44)			; selects current color number for write
psel		=	(VBXE_BASE+$45)			; selects the pallette number to load into
cr		=	(VBXE_BASE+$46)			; red component value
cg		=	(VBXE_BASE+$47)			; green component value
cb		=	(VBXE_BASE+$48)			; blue component value

; video control register (WRITE to $D640)
; bit 0 - xdl_enabled - set to enable XDL
; bit 1 - xcolor - set to enable 16 luminances and separate ANTIC hi-res foreground and background hues
; bit 2 - no_trans - clear means color 0 is transparent
; bit 3	- trans15 - allows additional transparent colors. only works when no_trans is 0

video_control	=	(VBXE_BASE+$40)

colmask		=	(VBXE_BASE+$49)			; AND mask for collisions with GTIA/ANTIC and VBXE stuff
colclr		=	(VBXE_BASE+$4A)			; clears coldetect (any value written here)
coldetect	=	(VBXE_BASE+$4A)			; collisions detected.
blt_adr		=	(VBXE_BASE+$50)			; blitter list address (3 bytes, consecutive)
blt_collision	=	(VBXE_BASE+$50)			; collision codes that blitter has set
blt_start	=	(VBXE_BASE+$53)			; bit 0 set to start blitter
blt_busy	=	(VBXE_BASE+$53)			; bit 1 for busy, bit 0 for BCB load. register will read 0 when blitter is stopped.
irq_control	=	(VBXE_BASE+$54)			; set bit 0 to enable blitter IRQ
irq_status	=	(VBXE_BASE+$54)			; bit 0 set means blitter threw an IRQ
P0		=	(VBXE_BASE+$55)			; priority select registers when attribute map is enabled.
P1		=	(VBXE_BASE+$56)
P2		=	(VBXE_BASE+$57)
P3		=	(VBXE_BASE+$58)
.struct	BCB
src_adr		.byte	3
src_step_y	.word
src_step_x	.byte
dest_adr	.byte	3
dest_step_y	.word
dest_step_x	.byte
blt_width	.word
blt_height	.byte
blt_and_mask	.byte
blt_xor_mask	.byte
blt_coll_mask	.byte
blt_zoom	.byte
pattern_feature	.byte
blt_control	.byte
.endstruct