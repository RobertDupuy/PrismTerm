;######################################################################################################################################
;
;
;	   ##	 ##   #	  ####	  ###		 #     # #####	 #     # ######
;	  #  #	 # #  #	 #    #	   #		 #     #  #   #	  #   #	  #   #
;	 #    #	 #  # #	 #	       #	     #     #  #   #	   # #	  #   
;	 #    #	 #   ##	  ####	   #	     #     #  ####	    #     ####
;	 ######	 #    #	      #	   #		  #   #	  #   #	   # #	  #   
;	 #    #	 #    #	 #    #	   #		   # #	  #   #	  #   #	  #   #
;	 #    #	 #    #	  ####	  ###		    #	 #####	 #     # ######
;
;	 ####### ######	 #####	 ##   ##  ###	 ##   #	   ##	 ###
;	 #  #  #  #   #	  #   #	 # # # #   #	 # #  #	  #  #	  #
;	    #	  # #	  #   #	 #  #  #   #	 #  # #	 #    #	  #
;	    #	  ###	  ####	 #     #   #	 #   ##	 #    #	  #
;	    #	  # #	  # #	 #     #   #	 #    #	 ######	  #
;	    #	  #   #	  #  #	 #     #   #	 #    #	 #    #	  #   #
;	   ###	 ######	 ###  #	 #     #  ###	 #    #	 #    #	 ######              !!CA65!!
;
;	Converted by:     Brad Colbert
;	Original MADS by: Joseph Zatarski
;	Version: v0.25
;
;	terminal emulator that supports ANSI/ECMA-48 control sequences and a 256 character font
;######################################################################################################################################
;
; TODO:	(from Joey Z) add recieve buffer so that I don't call CIOV to do one character at a time. (done)
;	label scroll contains some unnecesary math (refer to comments near it) (done)
;	add stuff beyond just the C0 (ASCII) and C1 control character/sequence sets (this means finally starting on the cool stuff)
;		SGR - set graphics rendition (color, high intensity, etc.) - done
;		add J code - ED - erase in display (modes 0, 1, 2) - done
;		add A code - CUU - cursor up - done
;		add B code - CUD - cursor down - done
;		add C code - CUF - cursor forward (right) - done
;		add D code - CUB - cursor back (left) - done
;		add H code - CUP - cursor position (absolute) - done
;		add K code - EL - erase in line (modes 0, 1, 2) - done
;		add s code - SCP - save cursor position - done
;		add u code - RCP - restore cursor position - done
;		add f code - HVP - horizontal/vertical position (alias for CUP) - done
;		add E code - CNL - cursor next line - done
;		add F code - CPL - cursor previous line - done
;		add G code - CHA - cursor horizontal absolute - done
;		add S code - SU - scroll up - done
;		add T code - SD - scroll down - done
;

	.setcpu "6502"
    .macpack longbranch
	.feature labels_without_colons
	.feature org_per_seg

	.include "atarios_ca65.inc"		; atari OS equates,
	.include "atarihardware_ca65.inc"	; general atari hardware equates,
	.include "VBXE_ca65.inc"		; and VBXE equates

        .import __BSS_RUN__, __BSS_SIZE__
	.import	_vbxe_init, _vbxe_load_files, _vbxe_load_font, _vbxe_shutdown
	.import	_vbxe_font_from_rom

; VBXE equates
vbxe_mem_base	= $A000				; If I put it here, it should be OK and it won't conflict with the extended RAM.

vbxe_screen_top	= vbxe_mem_base + 256		; points to where the first character of the screen would be

; Runtime buffers are linker-managed BSS, after every linked object.
; Upstream equated recv_buffer to the main object's end, overwriting vbxe_lib.
; Display Handler Equates (among others)

cursor_address	= $80				; points to the address where the text cursor points
text_color	= $82				; current text color to be put on the screen.
row		= $83				; cursor row
column		= $84				; cursor column

src_ptr		= $85				; temporary source pointer on the zero page for memory moves
dst_ptr		= $87				; same as above, but for destination
counter		= $89				; byte counter for above

ctrl_seq_flg	= $8B				; bit 7 indicates escape received, bit 6 indicates CSI received,
						; bit 5 indicates "eat next byte" (final byte of an ESC + intermediate
						; sequence such as ESC ( @ or ESC # 8). When bit 5 is paired with bit 7,
						; the BIT dispatch in process_char routes through the escape branch.
						; bit 4 indicates "string mode" — body of a DCS/SOS/OSC/PM/APC
						; sequence; bytes are consumed until BEL ($07) or ST (ESC \).
						; In string mode, bit 5 alongside bit 4 means the previous byte was
						; ESC and the current byte is the ST final byte (consume + exit).

cursor_flg	= $8C				; bit 7 indicates whether or not the cursor is currently visible
						; that is, if it's a 1, then the color of the current character
						; has been inverted to show the cursor.
temp_char	= $8D				; holds a character temporarily for the recieve processing routines

temp_key_char	= $8E

recvbuflen	= $8F				; holds the number of bytes in the receive buffer that are waiting to be processed.

sendbufstart	= $90				; holds the position of the last character taken from the send FIFO
sendbufend	= $91				; holds the position of the last character added to the send FIFO
						; if these are equal, the FIFO is empty. if sendbufend is one less than sendbufstart
						; then the buffer is full. This always leaves a pad of one character which won't be
						; used between the start and end, but otherwise you end up with the two pointers being
						; equal as an ambiguous case. it could mean the buffer is full, or it's empty. This
						; means implementing another flag and set of conditionals to indicate and detect this.

ctrl_seq_index	= $92				; points to the current position in the control sequence buffer.

final_byte	= $93				; holds the control sequence final byte
inter_byte	= $94				; holds the control sequence intermediate byte
parameter_val	= $95				; holds a single parameter value
saved_row	= $96				; saved cursor row for SCP/RCP
saved_column	= $97				; saved cursor column for SCP/RCP
saved_cur_lo	= $98				; saved cursor_address low byte
saved_cur_hi	= $99				; saved cursor_address high byte
device_type	= $9A				; 0 = R: serial, 1 = N: FujiNet
n_err_code	= $9B				; saved error code from open_n_device
n_unit		= $9C				; FujiNet unit number (1–8), parsed from URL
n_trip		= $9D				; PROCEED interrupt trip flag (0=idle, 1=FujiNet has data)
lf_mode		= $9E				; LF-as-CRLF flag (0=LF only, 1=LF implies CR+LF)
saved_sdmctl	= $9F				; SDMCTL value saved at startup, restored on exit

; Settings menu state (used at device_select only — kbd_irq diverts keys
; here while menu_active=1; we own VKEYBD for the duration of device_select).
menu_active	= $A0				; 1 while menu mechanism is active
menu_key	= $A1				; raw KBCODE captured by kbd_irq
menu_key_ready	= $A2				; 1 = menu_key holds a fresh key
						; NOTE: $A3-$AF are menu_row..ps_str (see near menu_open).
						; The OPTION menu is reachable mid-session, so nothing that
						; must survive a menu invocation may live in $A3-$AF.

; Scrolling-region state and scratch for the CPU row-move primitives.
; Deliberately above the menu block: scroll_top/scroll_bot must survive an OPTION menu.
; scr_src/scr_dst must NOT reuse src_ptr/dst_ptr/counter: print_str keeps its string
; pointer in src_ptr, and read_line_vbxe keeps its buffer index in counter, both live
; across jsr process_char — which can reach a scroll.
scroll_top	= $B0				; 0-based top row of the DECSTBM region (default 0)
scroll_bot	= $B1				; 0-based bottom row of the DECSTBM region (default 23)
scr_src		= $B2				; 2 bytes — row-copy source, scroll primitives only
scr_dst		= $B4				; 2 bytes — row-copy destination / blank target

saved_portb	= $B6				; PORTB as found at startup (_vbxe_init banks BASIC out
						; so the MEMAC window at $A000 is not shadowed by ROM;
						; restore_os_hooks puts the original value back on exit)

; Terminal-mode state. Like the scroll region above, these live past $AF so they
; survive an OPTION menu invocation ($A3-$AF is menu-owned scratch).
term_last_col	= $B7				; highest column the auto-wrap may reach: 79 in ANSI,
						; 39 in ATASCII by default. Bounds wrapping ONLY —
						; absolute positioning and erase/scroll stay physical-80.
atascii_mode	= $B8				; 0 = ANSI/CP437, 1 = ATASCII. Selects the whole byte
						; pipeline: dispatcher, glyph translation, key encoding.
atascii_esc	= $B9				; 1 = ATASCII ESC ($1B) seen; print the next byte
						; literally instead of interpreting it as a control.
atascii_inv	= $BA				; $00 or $80 — sticky keyboard inverse-video latch,
						; toggled by the Atari inverse key, EORed into sent bytes.
active_font	= $BB				; main_menu item index of the font currently loaded,
						; drawn with a '*' marker by menu_redraw_items.

SOUNDR		= $41				; OS SIO bus sound enable (0 = silent)

	.segment "CODE"
	.org	$2800				; start of program
; apparently this is safe for most DOSes

;###################################################################################################################
; VBXE initialization
; check core version. needs to be fx core.
start
        jsr plus_bss_init
		jsr	_vbxe_init
		bne	vbxe_ok

; print an error message in case of missing or non fx core VBXE
; just set the ICBA and call another routine since printing to E: will be used twice.
		lda	#$00
		sta	memac_bank_sel		; close MEMAC window if it was left open
		sta	memac_control		; disable MEMAC CPU mapping
		sta	video_control		; disable VBXE video path
		lda	#$22
		sta	SDMCTL			; restore a normal ANTIC text DMA mode
		lda	#<no_vbxe_msg
		sta	ICBA
		lda	#>no_vbxe_msg
		sta	ICBA + 1
		jmp	print_error

vbxe_ok
		lda	#<vbxe_load_cfg
		ldx	#>vbxe_load_cfg
		jsr	_vbxe_load_files
		bne	vbxe_load_ok

; library file-load failed; force ANTIC text mode and show a startup error.
		lda	#$00
		sta	memac_bank_sel
		sta	memac_control
		sta	video_control
		lda	#$22
		sta	SDMCTL
		lda	#<load_fail_msg
		sta	ICBA
		lda	#>load_fail_msg
		sta	ICBA + 1
		jmp	print_error

vbxe_load_ok
		jmp	init_terminal_state
		
print_error					; prints the error message already set in ICBA
		lda	#$FF
		sta	ICBL
		lda	#09
		sta	ICCOM
		ldx	#$00
		jsr	CIOV

; wait for return press

		lda	#$06
		sta	ICBA+1
		lda	#$00
		sta	ICBA
		sta	ICBL
		sta	ICBL+1
		lda	#$05
		sta	ICCOM
		jsr	CIOV

; go back to DOS
		jmp	exit_to_dos


no_vbxe_msg					; message to display for missing VBXE or non-fx core
		.byte	"No VBXE or non-fx core. Press return to continue.", $9B

load_fail_msg					; message to display if library load step fails
		.byte	"VBXE startup load failed. Press return to continue.", $9B

vbxe_load_cfg					; vbxe_load_cfg_t for _vbxe_load_files
		.addr	font_path
		.addr	pallette_path
		.addr	xdl
		.word	bcb_end - xdl

;###################################################################################################################
; now we start initializing the variables for the terminal state
; initialize the cursor address to be at the home position
init_terminal_state

		lda	#<vbxe_screen_top
		sta	cursor_address
		lda	#>vbxe_screen_top
		sta	cursor_address + 1

; initialize the text color

		lda	#$87			; $87 is white on black
		sta	text_color

; initialize the cursor position
		lda	#$00
		sta	row
		sta	column
		
; the flags for escape and CSI
		sta	ctrl_seq_flg

; the flag for the cursor
		sta	cursor_flg		; cursor is not on yet.

; indexes for send buffer
		sta	sendbufstart
		sta	sendbufend

; device type: 0 = R:, 1 = N:
		sta	device_type

; Settings menu — start inert; device_select activates it for its lifetime
		sta	menu_active
		sta	menu_key_ready

; Save OS vectors/PACTL once so exit_to_dos can always restore cleanly.
		lda	VKEYBD
		sta	old_vkeybd
		lda	VKEYBD+1
		sta	old_vkeybd+1
		lda	VPRCED
		sta	n_old_vprced
		lda	VPRCED+1
		sta	n_old_vprced+1
		lda	PACTL
		sta	n_old_pactl

; Install RESET warmstart hook so pressing RESET restores ANTIC/VBXE state first.
		lda	DOSINI
		sta	old_dosini
		lda	DOSINI+1
		sta	old_dosini+1
		lda	#<proto_reset
		sta	DOSINI
		lda	#>proto_reset
		sta	DOSINI+1

; Remember the OS SIO bus sound setting. The actual silencing is done per-device in
; device_open (N: only) and undone in device_select, so an R: session is left alone.
; restore_os_hooks and reset_cleanup both put this value back on the way out.
		lda	SOUNDR
		sta	saved_soundr

; LF-as-CRLF mode: default on (most hosts send bare LF expecting terminal to add CR)
		lda	#$01
		sta	lf_mode

; Terminal mode: ANSI, 80-column auto-wrap, no pending ATASCII escape, inverse off.
		lda	#$00
		sta	atascii_mode
		sta	atascii_esc
		sta	atascii_inv
		lda	#79
		sta	term_last_col

; _vbxe_load_files boots the font named by vbxe_load_cfg (ATARIPC.FNT), so the
; menu's '*' marker starts on that row.
		lda	#MENU_ITEM_ATARI
		sta	active_font
		jsr	menu_init_labels	; render the Mode:/Width: value rows for their initial state

		jsr plus_init
        jsr plus_expand_fonts
		jsr	reset_margins		; scrolling region starts as the full screen

		lda	#$00

; turn the cursor on
; normally, the screen windows starts in a state of all 0.
; that is fine for the characters
; but for the colors, this means the overlay bit is not set
; so all the colors are transparent and the cursor doesn't show up right
; so we clear the page first, which fills the page with null and the default color
		jsr	scroll_page
		jsr	cursor_on

;###################################################################################################################
; demo it by writing a file to the screen (for now at least)
; open the test file
;
;		ldx	#$10
;		lda	#$03
;		sta	ICCOM+$10
;		lda	#<test_file
;		sta	ICBA+$10
;		lda	#>test_file
;		sta	ICBA+$11
;		lda	#$04
;		sta	ICAX1+$10
;		lda	#$00
;		sta	ICAX2+$10
;		jsr	CIOV
;
;		lda	RTCLOK
;		sta	starttime
;		lda	RTCLOK + 1
;		sta	starttime + 1
;		lda	RTCLOK + 2
;		sta	starttime + 2

;###################################################################################################################
; device selection: prompt the user to choose R: serial or N: FujiNet

device_select
        lda #0
        sta telnet_on
        sta tn_state
		jsr	scroll_page		; clear screen
		lda	#$00			; move cursor to top-left
		sta	row
		sta	column
		sta	dev_ready		; no device open at the selection gate (cold start / disconnect / menu dismiss)
		jsr	reset_margins		; a new session must not inherit a stale scrolling region
		lda	saved_soundr		; an N: session silences the SIO bus sound; put it back
		sta	SOUNDR			; (no-op at cold start, where saved_soundr was just read)
		jsr	recalc_cursor
		lda	#<banner_msg
		ldx	#>banner_msg
		jsr	print_str
		jsr	CR_adr
		jsr	LF_adr
		lda	#<select_prompt
		ldx	#>select_prompt
		jsr	print_str

; close IOCB 2 so its state is clean for choose_n / choose_r to (re)open K: later
		ldx	#$20
		lda	#$0C
		sta	ICCOM+$20
		jsr	CIOV

; Install our kbd_irq for the duration of device_select. menu_active=1 makes
; kbd_irq stash raw KBCODE into menu_key/menu_key_ready instead of trying to
; queue into send_buffer. This lets us poll keypresses synchronously here AND
; gives the settings menu (which expects menu_key/menu_key_ready) something to
; read. The OS VKEYBD is restored before falling through to choose_n/choose_r
; so the K: CIO calls that follow (read_line_vbxe etc.) work correctly.
		sei
		lda	#<kbd_irq
		sta	VKEYBD
		lda	#>kbd_irq
		sta	VKEYBD+1
		cli
		lda	#$01
		sta	menu_active
		lda	#$00
		sta	menu_key_ready		; drop any stale key

        lda ui_ready
        beq @select_loop
        lda #0
        jsr ui_open
        jsr plus_select_actions
@select_loop
        jsr plus_select_idle
		lda	CONSOL
		and	#$04
		bne	@check_key
		; OPTION held — bring up the settings menu, then redraw
		lda	#<main_menu
		ldx	#>main_menu
		jsr	menu_open
		jmp	device_select		; full redraw after dismiss

@check_key
		lda	menu_key_ready
		beq	@select_loop
		ldx	menu_key
		lda	#$00
		sta	menu_key_ready
		; X = raw KBCODE — translate via keycode_table to letter we can compare
		lda	keycode_table,x
		cmp	#'N'
		beq	@go_n
		cmp	#'n'
		beq	@go_n
		cmp	#'R'
		beq	@go_r
		cmp	#'r'
		beq	@go_r
		jmp	@select_loop		; ignore other keys

@go_n
		jsr	uninstall_kbd_irq_for_select
		jmp	choose_n

@go_r
		jsr	uninstall_kbd_irq_for_select
		jmp	choose_r

uninstall_kbd_irq_for_select
		sei
		lda	old_vkeybd
		sta	VKEYBD
		lda	old_vkeybd+1
		sta	VKEYBD+1
		cli
		lda	#$00
		sta	menu_active
		sta	menu_key_ready
		rts

choose_r
; echo 'R' to VBXE, close K:, open R: serial device
		lda	#$00
		sta	device_type		; R: = 0 (clear in case we're retrying after a failed N: attempt)
		lda	#'R'
		sta	temp_char
		jsr	process_char
		jsr	CR_adr
		jsr	LF_adr
		ldx	#$20
		lda	#$0C			; CMD_CLOSE K:
		sta	ICCOM+$20
		jsr	CIOV
		jsr	open_r_device
		jmp	device_open

;choose_quit
;; Close K: on IOCB 2, clean up VBXE, and exit to DOS.
;		ldx	#$20
;		lda	#$0C			; CMD_CLOSE K:
;		sta	ICCOM+$20
;		jsr	CIOV
;		jmp	exit_to_dos

choose_n
; Echo 'N', newline, then step the user through connection details.
		lda	#'N'
		sta	temp_char
		jsr	process_char
		jsr	CR_adr
		jsr	LF_adr

; Always N: unit 1, device type N:
		lda	#$01
		sta	n_unit
		sta	device_type

; Clear caps lock (Atari: unshifted=uppercase, Shift=lowercase)
		lda	#$00
		sta	SHFLOK

; --- PROTOCOL ---
proto_again	lda	#<proto_prompt		; "[T]elnet [R]aw TCP [S]SH: "
		ldx	#>proto_prompt
		jsr	print_str
		jsr	open_k_iocb2
		ldx	#$20
		lda	#$07			; GET_CHARS — one char
		sta	ICCOM+$20
		lda	#<temp_char
		sta	ICBA+$20
		lda	#>temp_char
		sta	ICBA+$21
		lda	#$01
		sta	ICBL+$20
		lda	#$00
		sta	ICBL+$21
		jsr	CIOV
		ldx	#$20			; close K: IOCB 2
		lda	#$0C
		sta	ICCOM+$20
		jsr	CIOV
		lda	temp_char
		and	#$DF			; force uppercase
		cmp	#'T'
		beq	proto_tcp
        cmp #'R'
        beq proto_raw
		cmp	#'S'
		beq	proto_ssh
		jmp	proto_again		; invalid key, retry

proto_tcp	lda	#'T'
		jmp	proto_store
proto_raw lda #'R'
        jmp proto_store
proto_ssh	lda	#'S'
proto_store	sta	proto_byte
		sta	temp_char
		jsr	process_char		; echo choice letter
		jsr	CR_adr
		jsr	LF_adr

; --- SERVER ---
		lda	#<server_prompt		; "SERVER: "
		ldx	#>server_prompt
		jsr	print_str
		jsr	open_k_iocb2
        lda #63
        sta input_limit
		lda	#<server_buf
		sta	src_ptr
		lda	#>server_buf
		sta	src_ptr+1
		jsr	read_line_vbxe
		jsr	CR_adr
		jsr	LF_adr

; --- PORT ---
		lda	#<port_prompt		; "PORT (Enter for default): "
		ldx	#>port_prompt
		jsr	print_str
		jsr	open_k_iocb2
        lda #7
        sta input_limit
		lda	#<port_buf
		sta	src_ptr
		lda	#>port_buf
		sta	src_ptr+1
		jsr	read_line_vbxe
		jsr	CR_adr
		jsr	LF_adr

; Skip credentials for TCP (telnet) — only SSH needs them
		lda	proto_byte
		cmp	#'S'
		bne	build_url

; --- USER ---
		lda	#<user_prompt		; "USER: "
		ldx	#>user_prompt
		jsr	print_str
		jsr	open_k_iocb2
        lda #63
        sta input_limit
		lda	#<login_buf
		sta	src_ptr
		lda	#>login_buf
		sta	src_ptr+1
		jsr	read_line_vbxe
		jsr	CR_adr
		jsr	LF_adr

; --- PASSWORD ---
		lda	#<pw_prompt		; "PASSWORD: "
		ldx	#>pw_prompt
		jsr	print_str
		jsr	open_k_iocb2
        lda #63
        sta input_limit
		lda	#<password_buf
		sta	src_ptr
		lda	#>password_buf
		sta	src_ptr+1
		jsr	read_line_vbxe_pw	; echoes asterisks
		jsr	CR_adr
		jsr	LF_adr

; --- BUILD URL ---
build_url
; Construct "N1:TCP://server:port" or "N1:SSH://server:port" in n_url_buf.
		lda	#<n_url_buf
		sta	dst_ptr
		lda	#>n_url_buf
		sta	dst_ptr+1

		lda	#<n1_str		; "N1:"
		sta	src_ptr
		lda	#>n1_str
		sta	src_ptr+1
		jsr	copy_str

		lda	proto_byte
		cmp	#'S'
		beq	url_ssh
		lda	#<tcp_url_str		; "TCP://"
		sta	src_ptr
		lda	#>tcp_url_str
		sta	src_ptr+1
		jmp	url_proto_done
url_ssh		lda	#<ssh_url_str		; "SSH://"
		sta	src_ptr
		lda	#>ssh_url_str
		sta	src_ptr+1
url_proto_done	jsr	copy_str

		lda	#<server_buf		; hostname
		sta	src_ptr
		lda	#>server_buf
		sta	src_ptr+1
		jsr	copy_str

		lda	#':'			; separator
		jsr	put_byte_dst

		lda	port_buf		; did user enter a port?
		cmp	#$9B
		bne	url_user_port
		lda	proto_byte		; no — use default
		cmp	#'S'
		beq	url_ssh_default
		lda	#<tcp_port_str		; "23"
		sta	src_ptr
		lda	#>tcp_port_str
		sta	src_ptr+1
		jmp	url_port_done
url_ssh_default	lda	#<ssh_port_str		; "22"
		sta	src_ptr
		lda	#>ssh_port_str
		sta	src_ptr+1
		jmp	url_port_done
url_user_port	lda	#<port_buf
		sta	src_ptr
		lda	#>port_buf
		sta	src_ptr+1
url_port_done	jsr	copy_str

		lda	#$9B			; FujiNet URL terminator
		jsr	put_byte_dst

; pre-configure SSH credentials via $FD/$FE before open (SSH only)
		lda	proto_byte
		cmp	#'S'
		bne	skip_nlogin
		jsr	nlogin_n_device
skip_nlogin

		lda	#<connecting_msg
		ldx	#>connecting_msg
		jsr	print_str

		jsr	open_n_device
		bmi	n_open_failed

		lda	#<n_open_ok_msg
		ldx	#>n_open_ok_msg
		jsr	print_str
		jmp	device_open

n_open_failed
		sty	n_err_code		; save CIO error code before any other call corrupts Y
		lda	#<no_n_msg
		ldx	#>no_n_msg
		jsr	print_str		; "FujiNet open failed: $"
		lda	n_err_code
		jsr	print_hex_byte		; display two hex digits of error code
		lda	#<press_return_msg
		ldx	#>press_return_msg
		jsr	print_str
		jmp	wait_for_return

wait_for_return
		lda	#$05			; GET_REC on IOCB 0 — waits for Enter
		sta	ICCOM
		lda	#<select_buf
		sta	ICBA
		lda	#>select_buf
		sta	ICBA+1
		lda	#$04
		sta	ICBL
		lda	#$00
		sta	ICBL+1
		jsr	CIOV
		jmp	device_select

device_open
        jsr telnet_start
; a device (R: or N:) is now open — enable the R: font-swap serial reconfigure
		lda	#$01
		sta	dev_ready

; N: drives a raw SIO transaction (jsr SIOV) for every status poll, every read and every
; coalesced keystroke batch, so the OS SIO bus sound — POKEY channel 4 left audible while
; channels 3+4 clock the serial bit stream — whines continuously. Silence it for the life
; of the N: session. R: streams over CIO concurrent mode and never enters the OS SIO sound
; path, so leave its setting untouched. device_select restores saved_soundr on the way out
; (disconnect, menu dismiss); restore_os_hooks and reset_cleanup cover exit and RESET.
		lda	device_type
		beq	keep_sio_sound		; 0 = R:
		lda	#$00
		sta	SOUNDR
keep_sio_sound

; flush any keystrokes buffered during device selection
		lda	#$00
		sta	sendbufstart
		sta	sendbufend
; save old VKEYBD then install our keyboard handler
		sei				; disable interrupts while changing vectors
		lda	VKEYBD
		sta	old_vkeybd
		lda	VKEYBD+1
		sta	old_vkeybd+1
		lda	#<kbd_irq
		sta	VKEYBD
		lda	#>kbd_irq
		sta	VKEYBD+1
		cli				; re-enable interrupts

; for N: install PROCEED interrupt so FujiNet signals when data is available
		lda	device_type
		beq	wait_for_byte		; R: — no PROCEED interrupt needed

		lda	PACTL
		sta	n_old_pactl
		and	#$FE			; disable PROCEED IRQ while changing vector
		sta	PACTL
		lda	VPRCED
		sta	n_old_vprced
		lda	VPRCED+1
		sta	n_old_vprced+1
		lda	#<n_proceed_irq
		sta	VPRCED
		lda	#>n_proceed_irq
		sta	VPRCED+1
		lda	#$00
		sta	n_trip			; no data waiting yet
		lda	PACTL
		ora	#$01			; enable PROCEED IRQ
		sta	PACTL

wait_for_byte	jsr plus_select_actions
        jsr plus_idle
        jsr	check_sendbuf
		lda	CONSOL
		and	#$04
		bne	@no_option
; OPTION held — open the settings menu. kbd_irq is already installed by
; device_open for the session, but during normal terminal use menu_active=0 so
; keys go to the send-buffer FIFO. Flip menu_active=1 around menu_open so its
; modal loop sees menu_key_ready being set, then back to 0 so subsequent
; typing resumes routing to the FIFO. R: font swap is handled by the pre-CLOSE
; / post-OPEN dance in font_swap_prep_r and font_swap_restore_r.
		lda	#$01
		sta	menu_active
		lda	#$00
		sta	menu_key_ready
		lda	#<main_menu
		ldx	#>main_menu
		jsr	menu_open
		lda	#$00
		sta	menu_active
		sta	menu_key_ready
@no_option	lda	device_type
		beq	r_do_recv		; R: — always poll
		lda	n_trip			; N: — only recv when FujiNet has signalled data
		beq	wait_for_byte
r_do_recv	jsr	recv_from_device
		jmp	wait_for_byte		; n_trip clear + PROCEED re-arm now happen inside recv_from_device

handle_disconnect_n
; Tear down the PROCEED interrupt, close N:, print message, restart device selection.
		lda	PACTL
		and	#$FE
		sta	PACTL			; disable PROCEED IRQ
		lda	n_old_vprced
		sta	VPRCED
		lda	n_old_vprced+1
		sta	VPRCED+1		; restore old VPRCED vector
		lda	PACTL
		and	#$FE
		ora	n_old_pactl
		sta	PACTL			; restore old IRQ enable state
		jsr	nclose_n_device		; send CLOSE to FujiNet
; restore the OS keyboard IRQ so K: CIO reads work at device selection
		sei
		lda	old_vkeybd
		sta	VKEYBD
		lda	old_vkeybd+1
		sta	VKEYBD+1
		cli
		lda	#$00
		sta	device_type		; back to no device selected
		lda	#<disconnected_msg
		ldx	#>disconnected_msg
		jsr	print_str
		jsr	CR_adr
		jsr	LF_adr
		jmp	device_select

.proc recv_from_device
		lda	device_type
		bne	n_recv

; R: path — poll concurrent I/O status buffer
		ldx	#$10
		lda	#13			; CMD_STATUS
		sta	ICCOM+$10
		lda	#<r_path
		sta	ICBA+$10
		lda	#>r_path
		sta	ICBA+$11
		lda	#0
		sta	ICAX1+$10
		sta	ICAX2+$10
		jsr	CIOV
		jmp	check_dvstat

; N: path — SIO STATUS to check bytes waiting, then SIO READ
; Clear n_trip and re-arm PROCEED up front so any new burst arriving during
; STATUS/READ/process is caught on the next wait_for_byte pass. Covers all exit
; paths (no-data, disconnect, full read).
n_recv		lda	#$00
		sta	n_trip
		lda	PACTL
		ora	#$01
		sta	PACTL
		lda	#FUJI_ID
		sta	DDEVIC
		lda	n_unit
		sta	DUNIT
		lda	#'S'			; Status command
		sta	DCOMND
		lda	#$40			; read direction (FujiNet sends us 4 status bytes)
		sta	DSTATS
		lda	#<DVSTAT0
		sta	DBUFLO
		lda	#>DVSTAT0
		sta	DBUFHI
		lda	#FUJI_TIMEOUT
		sta	DTIMLO
		lda	#$00
		sta	DTIMHI
		lda	#$04			; 4 status bytes into DVSTAT0-DVSTAT3
		sta	DBYTLO
		lda	#$00
		sta	DBYTHI
		sta	DAUX1			; DAUX must be 0 for STATUS (not the byte count)
		sta	DAUX2
		jsr	SIOV
		bpl	n_recv_status_ok	; positive result → no SIO error
		jmp	n_recv_disconnect	; SIO error (timeout/NAK) → disconnected
n_recv_status_ok
		lda	DVSTAT2			; connection flag (C field): 0 = not connected
		bne	n_recv_dvstat_ok	; non-zero = still connected, proceed
		jmp	n_recv_disconnect	; C=0 → disconnected
n_recv_dvstat_ok
		lda	DVSTAT0			; bytes waiting (lo + hi)
		ora	DVSTAT1
		bne	n_recv_read
		jmp	done			; nothing waiting
n_recv_read

; Clamp to 255 bytes max
		lda	DVSTAT1
		bne	use_max
		lda	DVSTAT0
		bne	use_lo
use_max		lda	#$FF
use_lo		sta	recvbuflen		; how many bytes to read

; SIO READ
		lda	#FUJI_ID
		sta	DDEVIC
		lda	n_unit
		sta	DUNIT
		lda	#'R'			; Read command
		sta	DCOMND
		lda	#$40			; read direction
		sta	DSTATS
		lda	#<recv_buffer
		sta	DBUFLO
		lda	#>recv_buffer
		sta	DBUFHI
		lda	#FUJI_TIMEOUT
		sta	DTIMLO
		lda	#$00
		sta	DTIMHI
		lda	recvbuflen
		sta	DBYTLO
		sta	DAUX1
		lda	#$00
		sta	DBYTHI
		sta	DAUX2
		jsr	SIOV
		jmp	process_bytes

check_dvstat	lda	DVSTAT1			; DVSTAT1 and 2 hold the number of bytes in the input buffer
		ora	DVSTAT2
		jeq	done			; nothing waiting, return

; R: read bytes — request exactly DVSTAT bytes via GET_CHARS
		ldx	#$10
		lda	#$07
		sta	ICCOM+$10
		lda	#<recv_buffer
		sta	ICBA + $10
		lda	#>recv_buffer
		sta	ICBA + $11
		lda	DVSTAT2
		bne	maxbuflen
		lda	DVSTAT1
		jmp	storebuflen
maxbuflen	lda	#$FF
storebuflen	sta	ICBL + $10
		sta	recvbuflen
		lda	#$0
		sta	ICBL + $11
		lda	#$0D
		sta	ICAX1 + $10		; turns out you need this for GET or it doesn't work.
		jsr	CIOV

process_bytes	jsr cursor_off
        inc batch_mode
        jsr plus_prepare_fast
        ldy #0
next_byte	lda	recv_buffer, y		; get character from buffer
        sta temp_char
        ldx receive_fast
        beq slow_byte
        cmp #32
        bcc slow_byte
        cmp #127
        bcs slow_byte
        ; Common ASCII run: screen cursor is already hidden for the batch.
        sty receive_pos
        ldy #0
        sta (cursor_address),y
        iny
        lda text_color
        sta (cursor_address),y
        lda column
        cmp #79
        beq fast_edge
        inc column
        lda cursor_address
        clc
        adc #2
        sta cursor_address
        bcc fast_done
        inc cursor_address+1
        jmp fast_done
fast_edge:
        lda #1
        sta wrap_pending
        lda #0
        sta receive_fast
fast_done:
        ldy receive_pos
        jmp byte_done
slow_byte:
        tya
		pha				; push Y just in case

        jsr plus_network_byte
        jsr plus_prepare_fast

		pla
		tay				; get Y back

byte_done:
		iny				; next character

		tya				; every 32 bytes, drain queued keystrokes so typing
		and	#$1F			; doesn't wait for the whole batch to finish rendering
		bne	skip_pump
		tya
		pha
		jsr	check_sendbuf
		pla
		tay
skip_pump
		cpy	recvbuflen
		bne	next_byte
done		lda batch_mode
        beq :+
        dec batch_mode
        jsr plus_render_dirty
        jsr cursor_on
:       rts

n_recv_disconnect
; Pop the jsr recv_from_device return address, then jump to disconnect handler.
		pla
		pla
		jmp	handle_disconnect_n
.endproc

.proc process_char
        lda atascii_mode
        beq is_ansi
        jmp atascii_char
is_ansi jmp plus_process
.endproc

; Entry point that always uses the ANSI control interpretation, whatever the
; terminal mode is. For the program's OWN screen output — the banner, the
; prompts, the connection wizard — which is ANSI text we author, and which would
; otherwise be mangled in ATASCII mode ($1B there quotes the next byte, so the
; banner's colour sequences would print as literal "[31m" runs).
;
; Note this bypasses only the *parser* fork, not the glyph translation: put_byte
; still runs to_glyph, so the text is drawn with the correct screen codes for
; whichever font is loaded.
process_char_ansi = process_char::is_ansi
		
.proc do_ctrl_seq
        jmp plus_csi
.endproc


.proc put_byte
        jmp plus_put
.endproc

;###################################################################################################################
; character-encoding primitives (to_glyph, atascii_to_int, ascii_to_atascii).
; Hardware-free and shared verbatim with test/sim65/atascii_test.s.

PLUS_DIALOG_FONT = 1
		.include "atascii.inc"

;###################################################################################################################
; ATASCII byte pipeline
;
; Entered from process_char when atascii_mode is set, and it bypasses the
; ANSI/CSI state machine entirely — it has to, because $1B is ATASCII's
; quote-next-character escape and $9B is EOL, neither of which means what the
; ANSI parser would take it to mean.
;
; The ATASCII control set is exactly (c & $7F) in {$1B-$1F, $7D-$7F}. The plain
; and inverse-bit forms of each pair carry unrelated meanings ($1C is cursor-up,
; $9C is delete-line), so all sixteen dispatch separately. Everything else is a
; printable glyph — including the entire $80-$FF inverse range, which needs no
; special handling at all: to_glyph passes bit 7 through to the inverted half of
; the font that _vbxe_font_from_rom built.

.proc atascii_char
		lda	atascii_esc
		beq	check_ctrl
		lda	#$00			; previous byte was ESC — print this one
		sta	atascii_esc		; whatever it is, control code or not
		jmp	put_byte

check_ctrl	lda	temp_char
		and	#$7F
		cmp	#$1B
		bcc	print			; $00-$1A graphics characters
		cmp	#$20
		bcc	low_group		; $1B-$1F / $9B-$9F
		cmp	#$7D
		bcc	print			; $20-$7C text
						; $7D-$7F / $FD-$FF falls through

; --- $7D-$7F, and their inverse-bit forms $FD-$FF ---
		bit	temp_char		; N = bit 7 of the original byte
		bmi	high_inverse
		cmp	#$7D
		beq	do_clear		; $7D clear screen and home
		cmp	#$7E
		beq	t_backspace		; $7E destructive backspace
		jmp	HT_adr			; $7F tab

high_inverse	cmp	#$7D
		beq	do_bell			; $FD buzzer
		cmp	#$7E
		beq	t_del_char		; $FE delete character
		jmp	do_ins_char		; $FF insert character

; --- $1B-$1F, and their inverse-bit forms $9B-$9F ---
low_group	bit	temp_char
		bmi	low_inverse
		cmp	#$1B
		beq	do_esc			; $1B quote the next character
		cmp	#$1C
		beq	do_up			; $1C cursor up
		cmp	#$1D
		beq	do_down			; $1D cursor down
		cmp	#$1E
		beq	do_left			; $1E cursor left
		jmp	do_right		; $1F cursor right

low_inverse	cmp	#$1B
		beq	t_eol			; $9B end of line
		cmp	#$1C
		beq	t_del_line		; $9C delete line
		cmp	#$1D
		beq	t_ins_line		; $9D insert line
		rts				; $9E clear / $9F set tab stop. Tab stops are
						; fixed at 8 columns here, so these are no-ops
						; for the same reason HTS_adr/VTS_adr are.

print		jmp	put_byte
do_clear	jmp	FF_adr			; home + clear, exactly ANSI's form feed
do_bell		jmp	BEL_adr

; The editing handlers sit past the end of branch range from the chain above,
; so they are reached through these near trampolines.
t_backspace	jmp	do_backspace
t_del_char	jmp	do_del_char
t_eol		jmp	do_eol
t_del_line	jmp	do_del_line
t_ins_line	jmp	do_ins_line

do_esc		lda	#$01
		sta	atascii_esc
		rts

; --- cursor movement ---
;
; ATASCII cursor controls WRAP at the screen edges rather than clamping the way
; ANSI CUU/CUD/CUF/CUB do, and wrapping off the bottom returns to the top
; instead of scrolling. That is why none of the ANSI handlers can be reused.

do_up		jsr	cursor_off
		lda	row
		bne	@dec
		lda	#24			; wrap from the top row round to the bottom
@dec		sec
		sbc	#1
		sta	row
		jmp	reposition

do_down		jsr	cursor_off
		lda	row
		cmp	#23
		bcc	@inc
		lda	#$FF			; wrap from the bottom row back to the top
@inc		clc
		adc	#1
		sta	row
		jmp	reposition

do_left		jsr	cursor_off
		jsr	left_core
		jmp	reposition

do_right	jsr	cursor_off
		lda	column
		cmp	term_last_col
		bcc	@inc
		lda	#$00			; past the right margin: down a line, column 0
		sta	column
		lda	row
		cmp	#23
		bcc	@next_row
		lda	#$FF			; and off the bottom wraps to the top
@next_row	clc
		adc	#1
		sta	row
		jmp	reposition
@inc		inc	column
		jmp	reposition

.proc left_core					; column-- with wrap to the end of the previous row
; Shared by cursor-left and backspace. Adjusts row/column only — the caller is
; responsible for cursor_off, recalc_cursor and cursor_on.
		lda	column
		bne	@dec
		lda	term_last_col		; wrap to the right margin of the row above
		sta	column
		lda	row
		bne	@row_up
		lda	#24			; and off the top wraps to the bottom
@row_up		sec
		sbc	#1
		sta	row
		rts
@dec		dec	column
		rts
.endproc

reposition	jsr	recalc_cursor
		jmp	cursor_on

; --- editing ---

do_backspace					; move left, then erase the cell landed on
		jsr	cursor_off
		jsr	left_core
		jsr	recalc_cursor
		ldy	#$00
		lda	#$00			; internal $00 is space in the ROM charset
		sta	(cursor_address),y
		iny
		lda	text_color		; must be text_color, not $00: bit 7 is the
		sta	(cursor_address),y	; VBXE overlay opacity bit
		jmp	cursor_on

do_ins_line					; open a blank line at the cursor row
		lda	row			; ignore if the cursor is outside the region,
		cmp	scroll_top		; matching IL_adr
		bcc	bail
		lda	scroll_bot
		cmp	row
		bcc	bail
		jsr	cursor_off
		ldy	#$01
		lda	row
		ldx	scroll_bot
		jsr	scroll_rgn_down
		jmp	cursor_on

do_del_line					; pull the region up over the cursor row
		lda	row
		cmp	scroll_top
		bcc	bail
		lda	scroll_bot
		cmp	row
		bcc	bail
		jsr	cursor_off
		ldy	#$01
		lda	row
		ldx	scroll_bot
		jsr	scroll_rgn_up
		jmp	cursor_on

do_ins_char	jsr	cursor_off
		jsr	row_insert_char
		jmp	cursor_on

do_del_char	jsr	cursor_off
		jsr	row_delete_char
		jmp	cursor_on

do_eol		jsr	CR_adr			; EOL is unconditionally CR + LF, so enter the
		jmp	line_feed_no_cr		; line feed past its lf_mode check

bail		rts
.endproc

;###################################################################################################################
; control sequence handlers
SOH_adr						; Start of Header prints a character
STX_adr						; Start of Text prints a character
ETX_adr						; End of Text prints a character
EOT_adr						; End of Transmission prints a character
ACK_adr						; Acknowledge prints a character
SO_adr						; Shift Out prints a character
SI_adr						; Shift In prints a character
DLE_adr						; Data Link Escape prints a character
DC1_adr						; Device Control 1-4 print a character
DC2_adr
DC3_adr
DC4_adr
NAK_adr						; Non-Acknowledge prints a character
SYN_adr						; Synchronous Idle prints a character
ETB_adr						; End of Transmission Block prints a character
CAN_adr						; Cancel prints a character
EM_adr						; End of Medium prints a character
SUB_adr						; Substitute prints a character
IS1_adr						; Information Separator 1-4 print characters
IS2_adr
IS3_adr
IS4_adr
		jmp 	put_byte		; the put_byte routine will return for us
ESC_adr						; Escape sets the escape flag
		lda	#$80
		sta	ctrl_seq_flg
		rts
CSI_adr						; Control Sequence Introducer sets the CSI flag which causes interpretation of a control sequence to begin
		lda	#0
		sta	ctrl_seq_index
		lda	#$40
		sta	ctrl_seq_flg
		rts
VT_adr						; Vertical Tab, like LF
        jmp LF_adr
IND_adr
        jmp plus_index
.proc LF_adr
        lda lf_mode
        beq lf_no_cr
        jsr CR_adr
lf_no_cr jmp plus_index
.endproc

; Entry point into LF_adr past its lf_mode test, for callers that have already
; done the carriage return themselves (ATASCII EOL). Declared out here because
; atascii_char is assembled earlier in the file and a Scope::label reference
; cannot resolve before the scope exists.
line_feed_no_cr	= LF_adr::lf_no_cr

.proc CR_adr					; Carriage Return puts the cursor at the home position of the current line 
                                                ; (AKA, cursor gets column number * 2 bytes/char subtracted from it)
		jsr	cursor_off
		lda	cursor_address		; get cursor
		sec				; set carry for subtraction
		sbc	column			; do the subtraction
		bcs	no_borrow1		; carry will be clear if no borrow
		dec	cursor_address + 1	; otherwise it borrowed, so decrement cursor_address high byte
no_borrow1	sec				; prepare for another subtraction
		sbc	column			; do it
		bcs	no_borrow2		; borrow thing again...
		dec	cursor_address + 1
no_borrow2	sta	cursor_address		; store the result
		lda 	#00
		sta	column			; set column to 0
		jmp	cursor_on
.endproc

.proc BEL_adr
        lda #8
        sta bell_ticks
        rts
.endproc
		
FF_adr						; Form Feed clears the screen and sets cursor to the home position.
		jsr	cursor_off
		lda	#0			; home the cursor
		sta	row
		sta	column
		lda	#<vbxe_screen_top
		sta	cursor_address
		lda	#>vbxe_screen_top
		sta	cursor_address + 1
		jsr	scroll_page		; clear the screen
		jmp	cursor_on
		
.proc BS_adr					; Back Space moves the cursor left by one character (2 bytes).
		jsr	cursor_off
		lda	column
		bne	not_left		; if we're not at the left side yet, keep going
		rts				; but if we ARE at the left side, do nothing
not_left	dec	column			; if column isn't 0, we just decrement it
		lda	cursor_address
		bne	no_borrow		; if the cursor address low half isn't 0, we don't borrow
		dec	cursor_address + 1	; otherwise, borrow
no_borrow	dec	cursor_address		; if it weren't for the fact that cursor_address should always be even,
		dec 	cursor_address		; we would have to look for a borrow here too.
		jmp	cursor_on
.endproc
NUL_adr						; null does nothing
ENQ_adr						; Enquiry is probably supposed to return an ACK, but for now, it does nothing
HTS_adr						; tab-stop table not implemented; HT uses fixed 8-column stops
HTJ_adr
VTS_adr
BPH_adr						; BPH doesn't apply in this case.
NBH_adr						; nor do any of the following
SSA_adr
ESA_adr
PLD_adr
PLU_adr
RI_adr
        jmp plus_reverse_index
SS2_adr
SS3_adr
PU1_adr
PU2_adr
STS_adr
CCH_adr
MW_adr
SPA_adr
EPA_adr
SCI_adr
ST_adr
		rts

; String-introducer C1 controls (DCS/SOS/OSC/PM/APC) start a body whose
; contents must be consumed silently until BEL or ST (ESC \). Without this,
; the body — which may contain things like '[3;52H' from an embedded CSI —
; falls through to put_byte and prints as text. Enter string mode (bit 4).
DCS_adr
SOS_adr
OSC_adr
PM_adr
APC_adr		lda	#$10
		sta	ctrl_seq_flg
		rts
.proc HT_adr					; Horizontal Tab: advance to next 8-column stop
		jsr	cursor_off
		lda	column
		and	#$F8			; round down to current 8-col boundary
		clc
		adc	#8			; advance one stop
		cmp	term_last_col		; clamp at the right margin (79, or 39 in 40-column ATASCII)
		bcc	ok
		beq	ok
		lda	term_last_col
ok		sta	column
		jsr	recalc_cursor
		jmp	cursor_on
.endproc

NEL_adr						; Next Line is combination of CR and LF
		jsr	CR_adr
		jmp	LF_adr

.proc recalc_cursor
        ldx row
        lda column
        asl
        clc
        adc row_addr_lo,x
        sta cursor_address
        lda row_addr_hi,x
        adc #0
        sta cursor_address+1
        rts
.endproc

;###################################################################################################################
.include "plus_region.inc"

.proc scroll_1d					; scroll one down routine.
; uses the blitter to move everything up just one line.

		lda	memac_bank_sel		; get the old bank number (should be the last bank, but we can't assume)
		pha
		
		lda	#$80			; bank 0, so we can put the color byte as the fill byte
		sta	memac_bank_sel
		
		lda	text_color		; put the color in the fill pattern location.
		sta	scroll_1d_color-xdl+$0800+vbxe_mem_base
		
		lda	#<(bcb_one_down - xdl + $800)
		sta	blt_adr
		lda	#>(bcb_one_down - xdl + $800)
		sta	blt_adr+1
		lda	#^(bcb_one_down - xdl + $800)
		sta	blt_adr+2
		
		lda	#1
		sta	blt_start
		
loop		lda	blt_busy
		bne	loop
		
		pla
		sta	memac_bank_sel
		rts
.endproc
		
.proc scroll_page				; scroll one page
; used for FF and also to initialize the screen so the color is not all $00

		lda	memac_bank_sel		; get the old bank number (should be the last bank, but we can't assume)
		pha
		
		lda	#$80			; bank 0, so we can put the color byte as the fill byte
		sta	memac_bank_sel
		
		lda	text_color		; put the color in the fill pattern location.
		sta	clr_scr_color-xdl+$0800+vbxe_mem_base
		
		lda	#<(bcb_clr_scr - xdl + $800)
		sta	blt_adr
		lda	#>(bcb_clr_scr - xdl + $800)
		sta	blt_adr+1
		lda	#^(bcb_clr_scr - xdl + $800)
		sta	blt_adr+2
		
		lda	#1
		sta	blt_start
		
loop		lda	blt_busy
		bne	loop
		
		pla
		sta	memac_bank_sel
        lda #0
        ldx #24
        jmp plus_styles_blank
.endproc
		
.proc cursor_on					; turn on the cursor
; Cursor stays hidden during receive batches and DEC ?25l.
        lda batch_mode
        bne skip
        lda cursor_hidden
        bne skip
		bit	cursor_flg
		bpl	cursor_toggle
skip		rts
.endproc
		
.proc cursor_off				; turn off the cursor
; but ONLY if the cursor isn't already off
		bit	cursor_flg
		bmi	cursor_toggle
skip		rts
.endproc

cursor_toggle					; inverts the color of the current character to show the cursor
		ldy	#1
		lda	(cursor_address),y
		eor	#$77			; invert the color, but not bit 7 (transparency bit) or bit 3 (foreground intensity)
		sta	(cursor_address),y
		lda	cursor_flg
		eor	#$80			; flip the cursor flag
		sta	cursor_flg
        jmp plus_dirty_cursor
		
.proc kbd_irq
; some of this code is based on the OS keyboard IRQ code. It even reuses some of the same OS variables.

		lda	KBCODE			; get the key
		cmp	CH1			; and check if it's the same as last time
		bne	new_key			; if not, then it's definitely a new press
		lda	KEYDEL			; but if not, we need to see if it's too soon (and therefore a bounce)
		beq	new_key			; so if it is not, then it's a new press
		jmp	bounce			; otherwise, it's a bounce
		
new_key		txa
		pha				; we are going to need X
		tya
		pha				; ...and Y. The IRQ caller does not save it for us, and the
					; ATASCII paths below load it (ldy atascii_mode). Returning with
					; Y clobbered desyncs CIO's IOCB->ZP copy loop at $E4F3, which
					; indexes source by X and destination by Y: the resumed loop
					; then copies a mis-aligned slice into CIO's ZP work area and
					; CIO writes that garbage back over IOCB 1. Fatal on R:, which
					; sits in that loop almost continuously polling STATUS.
		lda	#$03			; we put this back in keydel so we can check for bounce again
		sta	KEYDEL
		lda	KBCODE			; if it's not, then we need the keycode again
		sta	CH1			; and we put it in here so we can check for bounce next time

; If menu mechanism is active (device_select context), divert raw KBCODE
; to menu_key/menu_key_ready instead of translating into the send_buffer FIFO.
		lda	menu_active
		beq	@no_menu
		lda	KBCODE
		sta	menu_key
		lda	#$01
		sta	menu_key_ready
		jmp	no_value
@no_menu

; I guess I will convert the key code to ASCII here, and interpret it, etc.
; I'll probably use an LUT
; for now, I guess I will just convert to ASCII and send. everything (for now) will become one character.
; return will produce CR I suppose. (for now only of course)

		lda	KBCODE			; reload — A was clobbered by the menu_active check above
		tax

; The Atari inverse-video key has no ASCII meaning, so keycode_table drops it
; (entries 39/103/167/231 are all 0). In ATASCII mode it becomes a sticky
; modifier instead, latching bit 7 onto everything typed after it.
		lda	atascii_mode
		beq	translate
		txa
		and	#$3F			; same key with any combination of shift/ctrl
		cmp	#KBCODE_INVERSE
		beq	toggle_inverse

translate	lda	keycode_table,x		; get the action byte from the table
		beq	no_value		; 0 means: do nothing
		bmi	send_seq		; bit 7 set means: emit an escape sequence (see escape_seq table)

		ldy	atascii_mode
		beq	push_byte
		jsr	ascii_to_atascii	; retarget the handful of keys ATASCII spells differently

push_byte	ldx	sendbufend		; single-byte path: get the current FIFO end
		inx				; advance to next slot
		cpx	sendbufstart		; if that equals start, the buffer is full
		beq	no_value		; drop the keypress
		stx	sendbufend		; commit new end
		sta	send_buffer,x		; store the byte
		jmp	no_value

toggle_inverse	lda	atascii_inv
		eor	#$80
		sta	atascii_inv
		jmp	no_value

send_seq	and	#$7F			; A = sequence index (0..n)

; ATASCII spells the arrows as single control codes rather than ESC [ x, so the
; whole three-byte path (and its free-slot check) is skipped.
		ldy	atascii_mode
		beq	ansi_seq
		cmp	#$04			; indices 0-3 are the four arrow keys
		bcs	no_value		; no other sequence has an ATASCII form
		tay
		lda	atascii_arrows,y
		jmp	push_byte

ansi_seq	sta	temp_key_char		; save N (adc can't add A to itself)
		asl				; A = 2N
		clc
		adc	temp_key_char		; A = 3N = byte offset into escape_seq

		pha				; save offset across the free-slot check
		lda	sendbufstart		; compute free slots = (start - end - 1) mod 256
		sec
		sbc	sendbufend
		sec
		sbc	#1
		cmp	#3
		pla				; restore offset
		bcc	no_value		; <3 free slots: drop the whole sequence (no partials)

		tax				; X = source offset into escape_seq
		tya				; preserve Y (IRQ caller doesn't save it for us)
		pha
		ldy	sendbufend
		iny
		lda	escape_seq,x
		sta	send_buffer,y
		iny
		inx
		lda	escape_seq,x
        cpx #12
        bcs :+
        lda app_cursor
        beq @normal_arrow
        lda #'O'
        bne :+
@normal_arrow: lda #'['
:       sta send_buffer,y
		iny
		inx
		lda	escape_seq,x
		sta	send_buffer,y
		sty	sendbufend
		pla
		tay

no_value	lda	#$00			; key down, so reset ATRACT counter
		sta	ATRACT			; not that it matters so much with VBXE, but it'll prevent changing border colors
		pla
		tay				; get y back (pushed last, so pulled first)
		pla
		tax				; get x back

bounce		lda	#$30			; we still set the repeat timer on a bounce I guess
		sta	SRTIMR			; that's the way the OS does it
		pla				; apparently the routine which jumps through the keyboard vector pushes A
		rti				; RTI because interupt
.endproc

; Raw KBCODE of the Atari inverse-video key, masked to its low six bits so the
; plain, shifted and control forms all match.
KBCODE_INVERSE	= 39

; ATASCII cursor controls, in the same order as escape_seq's arrow entries.
atascii_arrows	.byte	$1C			; $80 - cursor up
		.byte	$1D			; $81 - cursor down
		.byte	$1F			; $82 - cursor right
		.byte	$1E			; $83 - cursor left

; ascii_to_atascii lives in atascii.inc, included above with to_glyph.
		
.proc print_str
; Display a $9B-terminated string on the VBXE terminal.
; A = low byte of string address, X = high byte. Trashes A, X, Y.
; Advances src_ptr each iteration so process_char (which corrupts Y) is safe.
		sta	src_ptr
		stx	src_ptr+1
next_char	ldy	#$00
		lda	(src_ptr),y
		cmp	#$9B
		beq	done
		sta	temp_char
		jsr	process_char_ansi	; our own UI text is ANSI in every mode;
						; may corrupt all registers including Y
		inc	src_ptr
		bne	next_char
		inc	src_ptr+1
		jmp	next_char
done		rts
.endproc

.proc print_hex_byte
; Display a byte in A as two uppercase hex digits on the VBXE terminal.
; Uses the classic 6502 jsr-into-shared-tail trick: jsr hex_nibble for the
; high nibble returns here (via process_char's rts), then falls through to
; hex_nibble again for the low nibble, tail-calling process_char back to caller.
		pha
		lsr
		lsr
		lsr
		lsr			; high nibble in low 4 bits
		jsr	hex_nibble
		pla
		and	#$0F		; low nibble; fall through to hex_nibble
hex_nibble
		cmp	#10
		bcc	@digit
		adc	#6		; carry set from cmp, so +7 total; biases 10→'A'
@digit		adc	#'0'
		sta	temp_char
		jmp	process_char
.endproc

.proc open_k_iocb2
; Close then open K: on IOCB 2 for single-character reads. Called before each read_line_vbxe.
		ldx	#$20
		lda	#$0C			; CMD_CLOSE (harmless if already closed)
		sta	ICCOM+$20
		jsr	CIOV
		lda	#$03			; CMD_OPEN
		sta	ICCOM+$20
		lda	#<kbd_dev
		sta	ICBA+$20
		lda	#>kbd_dev
		sta	ICBA+$21
		lda	#$04			; OREAD
		sta	ICAX1+$20
		lda	#$00
		sta	ICAX2+$20
		jmp	CIOV
.endproc

.proc read_line_vbxe
; Read a line from K: (IOCB 2), echoing each character to the VBXE terminal.
; On entry: src_ptr = destination buffer. Buffer is $9B-terminated on return.
; Handles BS ($08). Closes K: (IOCB 2) when Enter is pressed.
; Uses counter ($89) as buffer index so Y corruption from CIOV/process_char is harmless.
		lda	#$00
		sta	counter			; buffer index
get_char	ldx	#$20
		lda	#$07			; GET_CHARS (one character, immediate)
		sta	ICCOM+$20
		lda	#<temp_char		; read directly into temp_char ($008D)
		sta	ICBA+$20
		lda	#>temp_char
		sta	ICBA+$21
		lda	#$01
		sta	ICBL+$20
		lda	#$00
		sta	ICBL+$21
		jsr	CIOV			; Y is corrupted by CIOV (returns status in Y)

		lda	temp_char
		cmp	#$9B			; Enter?
		beq	done
		cmp	#$7E			; Delete Back (Atari backspace key)
		beq	do_bs

		ldy	counter
        cpy input_limit
        bcs get_char
		sta	(src_ptr),y		; store character in buffer
		inc	counter
		beq	done			; buffer full (256 chars)
		jsr	process_char		; echo to VBXE (temp_char set, Y corruption OK)
		jmp	get_char

do_bs		lda	counter
		beq	get_char		; nothing to delete, ignore
		dec	counter
		jsr	BS_adr			; move cursor left
		lda	#' '
		sta	temp_char
		jsr	put_byte		; overwrite the character with a space
		jsr	BS_adr			; move cursor left again
		jmp	get_char

done		ldy	counter
		lda	#$9B
		sta	(src_ptr),y		; terminate buffer with EOL
		ldx	#$20
		lda	#$0C			; CMD_CLOSE K:
		sta	ICCOM+$20
		jsr	CIOV
		rts
.endproc

.proc read_line_vbxe_pw
; Like read_line_vbxe but echoes '*' for each character (password masking).
; On entry: src_ptr = destination buffer, K: open on IOCB 2.
		lda	#$00
		sta	counter
pw_get		ldx	#$20
		lda	#$07			; GET_CHARS
		sta	ICCOM+$20
		lda	#<temp_char
		sta	ICBA+$20
		lda	#>temp_char
		sta	ICBA+$21
		lda	#$01
		sta	ICBL+$20
		lda	#$00
		sta	ICBL+$21
		jsr	CIOV

		lda	temp_char
		cmp	#$9B			; Enter?
		beq	pw_done
		cmp	#$7E			; Delete Back (Atari backspace key)
		beq	pw_bs

		ldy	counter
        cpy input_limit
        bcs pw_get
		sta	(src_ptr),y		; store actual char in buffer
		inc	counter
		beq	pw_done			; buffer full
		lda	#'*'
		sta	temp_char
		jsr	put_byte		; echo '*'
		jmp	pw_get

pw_bs		lda	counter
		beq	pw_get
		dec	counter
		jsr	BS_adr
		lda	#' '
		sta	temp_char
		jsr	put_byte
		jsr	BS_adr
		jmp	pw_get

pw_done		ldy	counter
		lda	#$9B
		sta	(src_ptr),y
		ldx	#$20
		lda	#$0C			; CMD_CLOSE K:
		sta	ICCOM+$20
		jsr	CIOV
		rts
.endproc

; copy_str: copy $9B-terminated string from (src_ptr) to (dst_ptr), advance dst_ptr.
; Does NOT copy the $9B. Trashes A, Y.
.proc copy_str
		ldy	#$00
cs_loop		lda	(src_ptr),y
		cmp	#$9B
		beq	cs_done
		sta	(dst_ptr),y
		iny
		jmp	cs_loop
cs_done		tya
		clc
		adc	dst_ptr
		sta	dst_ptr
		bcc	cs_nc
		inc	dst_ptr+1
cs_nc		rts
.endproc

; put_byte_dst: write byte in A to (dst_ptr), advance dst_ptr. Trashes Y.
.proc put_byte_dst
		ldy	#$00
		sta	(dst_ptr),y
		inc	dst_ptr
		bne	pb_nc
		inc	dst_ptr+1
pb_nc		rts
.endproc

MAX_SEND_BATCH	= 64				; max bytes per SIO/CIO send call (bounds half-duplex stall)

.proc check_sendbuf				; drains queued keystrokes in one coalesced send.
; kbd_irq only mutates sendbufend; we only mutate sendbufstart — race-safe without sei.
; Drain up to MAX_SEND_BATCH bytes from send_buffer into send_stage_buf, then issue one
; SIO 'W' (N:) or one CIO PUT_CHARS (R:) for the whole run.

		lda	sendbufend
		sec
		sbc	sendbufstart		; A = pending count (mod 256, ring math)
		bne	have_data
		rts
have_data	cmp	#MAX_SEND_BATCH+1
		bcc	keep_count
		lda	#MAX_SEND_BATCH
keep_count	sta	send_count

		ldx	#$00
		ldy	sendbufstart
copy_loop	iny				; advance src before reading (mirrors kbd_irq's advance-before-write)
		lda	send_buffer,y
		sta	send_stage_buf,x
		inx
		cpx	send_count
		bne	copy_loop
		sty	sendbufstart		; commit: advance start by send_count (Y wrapped naturally)

		lda	device_type
		bne	n_send

; R: send — CIO PUT_CHARS on IOCB 1, multi-byte
		ldx	#$10
		lda	#$0B
		sta	ICCOM+$10
		lda	#<send_stage_buf
		sta	ICBA+$10
		lda	#>send_stage_buf
		sta	ICBA+$11
		lda	send_count
		sta	ICBL+$10
		lda	#$00
		sta	ICBL+$11
		lda	#$0D
		sta	ICAX1+$10
		jmp	CIOV

; N: send — SIO Write coalesced batch to FujiNet
n_send:
        lda #<send_stage_buf
        sta io_ptr
        lda #>send_stage_buf
        sta io_ptr+1
        lda send_count
        sta io_size
        lda #0
        sta io_size+1
        jmp wire_send
.endproc

;###################################################################################################################
; OPTION-key popup menu
;
; menu_open dispatches a self-contained modal menu intended for use at the
; device-selection screen, BEFORE any device is opened — so font loading via
; CIO disk doesn't compete with R:'s concurrent SIO traffic. Caller must have
; installed kbd_irq with menu_active=1 already (device_select does this); we
; read keys by polling menu_key_ready, which kbd_irq sets to a raw KBCODE.
; The screen region under the menu is saved into save_under_buf and restored
; byte-for-byte on dismiss.
;
; Menu data layout (immutable, in CODE segment):
;   .byte row, col, width, height, item_count
;   per item: .word label_ptr (null-terminated), action_ptr (jsr-able)
;
; ZP layout:
;   $A0-$A2 = IRQ state (menu_active / menu_key / menu_key_ready) — owned by kbd_irq
;   $A3-$AF = menu structure-loaded state, set up at menu_open entry
; Action procs may set menu_dismiss = 1 to tell the menu loop to exit
; immediately after the action returns (used by leaf actions like font load).

; Raw KBCODE values our kbd_irq diverts into menu_key while menu_active=1.
KBCODE_RETURN	= 12
KBCODE_ESC	= 28
KBCODE_UP	= 142
KBCODE_DOWN	= 143

; Unmodified '-' and '=' double as up/down. The Atari cursor keys are Ctrl+'-'
; and Ctrl+'=', and under emulation neither combination reliably reaches the
; machine: Altirra claims Ctrl+Minus for its own display zoom, and Ctrl+Equals
; comes through as $86 — Ctrl+'+', which is cursor *left*. With only $8E/$8F
; accepted the highlight cannot be moved at all, which reads as a hung menu.
; The bare keys arrive intact, so accept them too; nothing else in the menu
; wants '-' or '='.
KBCODE_UP_ALT	= 14				; '-' without Ctrl
KBCODE_DOWN_ALT	= 15				; '=' without Ctrl

; menu cell color attributes
MENU_COLOR_NORM	= $87				; white-on-black, overlay enabled (matches default text)
MENU_COLOR_HILT	= $F0				; black-on-white (MENU_COLOR_NORM EOR $77)

; Box-drawing slots. These are indices into box_glyphs_* below, NOT glyph codes:
; menu_draw_box writes raw glyph indices to the overlay, so it cannot go through
; to_glyph (which translates character codes) and instead needs a whole parallel
; glyph set for the ATASCII font. Fetch one with box_glyph.
BOX_UL		= 0				; upper-left  corner
BOX_UR		= 1				; upper-right corner
BOX_LL		= 2				; lower-left  corner
BOX_LR		= 3				; lower-right corner
BOX_VERT	= 4				; vertical edge
BOX_HORZ	= 5				; horizontal edge
BOX_SPACE	= 6				; box interior fill

; CP437 single-line box drawing. Stems use bit $10 / row 4, so corners and edges
; visually connect. True of every font shipped in fonts/ and disk/.
box_glyphs_cp437
		.byte	218, 191, 192, 217, 179, 196, 32

; The same box in Atari internal screen codes, for ATASCII mode. The Atari
; charset has a matching single-line set in its graphics range: ATASCII $11 $05
; $1A $03 $12 are the four corners and the horizontal bar, which become internal
; $51 $45 $5A $43 $52; the vertical uses the ordinary '|' at $7C. Internal $00 is
; space.
box_glyphs_atascii
		.byte	$51, $45, $5A, $43, $7C, $52, $00

; menu state ZP cells (above $A0-$A2 which hold the IRQ-state owned by kbd_irq:
; menu_active / menu_key / menu_key_ready — those are equated near the top of
; this file alongside the terminal state).
menu_row	= $A3
menu_col	= $A4
menu_w		= $A5
menu_h		= $A6
menu_count	= $A7
menu_selected	= $A8
menu_dismiss	= $A9
menu_ptr	= $AA				; 2 bytes — pointer to current menu structure
menu_action	= $AC				; 2 bytes — pointer used for jsr (menu_action)
ps_str		= $AE				; 2 bytes — string source pointer for menu_put_str_at (must be ZP for indirect-Y)


.proc menu_open
        ldy ui_ready
        beq :+
        lda #3
        jmp ui_open
:
; A = lo byte of menu structure, X = hi byte. Trashes most things.
		sta	menu_ptr
		stx	menu_ptr+1
        jsr cursor_off

; load row/col/w/h/count from the structure header
		ldy	#$00
		lda	(menu_ptr),y
		sta	menu_row
		iny
		lda	(menu_ptr),y
		sta	menu_col
		iny
		lda	(menu_ptr),y
		sta	menu_w
		iny
		lda	(menu_ptr),y
		sta	menu_h
		iny
		lda	(menu_ptr),y
		sta	menu_count
        jsr plus_center_menu

		lda	#$00
		sta	menu_selected
		sta	menu_dismiss
		sta	menu_key_ready		; drop any prior stale key

; start the highlight on the first row that is actually selectable, in case the
; menu opens with a divider or heading at the top
		ldx	#$00
@find_first	txa
		jsr	menu_get_action
		bne	@got_first
		inx
		cpx	menu_count
		bcc	@find_first
		ldx	#$00			; nothing selectable at all — degenerate, sit on row 0
@got_first	stx	menu_selected

; debounce: wait until OPTION is released before showing menu, otherwise
; the user's still-held press would immediately re-trigger after dismiss.
@wait_release
		lda	CONSOL
		and	#$05
        cmp #5
		bne	@wait_release

; snapshot the screen region we are about to overwrite, then draw
		jsr	save_under_rect
        lda #$fe
        ldx #0
        jsr plus_styles_snapshot
        jsr plus_menu_styles
		jsr	menu_draw

; main input loop — poll menu_key_ready (set by kbd_irq while menu_active=1).
; Caller (device_select) is responsible for installing kbd_irq with menu_active=1
; before the menu opens, and restoring on the way out of the device-select screen.
@loop
        jsr plus_render_dirty
		lda	menu_key_ready
		beq	@loop
		ldx	menu_key
		lda	#$00
		sta	menu_key_ready

		cpx	#KBCODE_UP
		beq	@key_up
		cpx	#KBCODE_UP_ALT
		beq	@key_up
		cpx	#KBCODE_DOWN
		beq	@key_down
		cpx	#KBCODE_DOWN_ALT
		beq	@key_down
		cpx	#KBCODE_RETURN
		beq	@key_select
		cpx	#KBCODE_ESC
		beq	@key_dismiss
		jmp	@loop			; ignore other keys

; Movement steps over non-selectable rows (dividers/headings, action_ptr $0000)
; rather than landing on them. If only dividers lie in the chosen direction the
; selection is left where it was.
@key_up
		ldx	menu_selected
@up_step	dex
		bmi	@loop			; ran off the top: nothing selectable up there
		txa
		jsr	menu_get_action
		beq	@up_step		; divider — keep going
		stx	menu_selected
		jsr	menu_redraw_items
		jmp	@loop

@key_down
		ldx	menu_selected
@down_step	inx
		cpx	menu_count
		bcs	@loop			; ran off the bottom
		txa
		jsr	menu_get_action
		beq	@down_step
		stx	menu_selected
		jsr	menu_redraw_items
		jmp	@loop

@key_select
		jsr	menu_invoke_action
		lda	menu_dismiss
		bne	@key_dismiss
		jmp	@loop

@key_dismiss
		jsr	restore_under_rect
        lda #$fe
        ldx #1
        jsr plus_styles_snapshot
        jsr cursor_on
        jsr plus_render_dirty
		lda	#$00
		sta	menu_key_ready		; drop stale dismiss key so device_select's poll doesn't see it
		rts
.endproc


.proc menu_get_action
; A = item index → menu_action = that item's action pointer.
; Returns with Z set when the pointer is $0000, which marks a non-selectable
; row: a divider or heading that the highlight skips over and RETURN ignores.
; Per-item record is 4 bytes: label_lo, label_hi, action_lo, action_hi.
; Records start at offset 5 (after the 5-byte header).
; Preserves X. Clobbers A, Y.
		asl
		asl				; * 4 (record size)
		clc
		adc	#5			; skip header
		tay
		iny
		iny				; advance past label_ptr to action_ptr
		lda	(menu_ptr),y
		sta	menu_action
		iny
		lda	(menu_ptr),y
		sta	menu_action+1
		ora	menu_action		; Z set iff both halves are zero
		rts
.endproc


.proc menu_invoke_action
; Look up action_ptr for menu_selected and JSR through it (so the action's
; rts returns here, not to the caller of menu_open).
		lda	menu_selected
		jsr	menu_get_action
		beq	@not_an_action		; divider row — RETURN does nothing
		jsr	@trampoline		; classic 6502 indirect-JSR via RTS-after-JMP
@not_an_action	rts
@trampoline
		jmp	(menu_action)
.endproc


.proc save_under_rect
; Copy menu_w * menu_h cells (2 bytes each) starting at (menu_row, menu_col)
; into save_under_buf. Uses src_ptr/dst_ptr.
		lda	menu_row
		ldx	menu_col
		jsr	menu_compute_addr	; → src_ptr

		lda	#<save_under_buf
		sta	dst_ptr
		lda	#>save_under_buf
		sta	dst_ptr+1

		lda	menu_h
		sta	su_rows_left

@row
		lda	menu_w
		asl				; bytes per row = w * 2
		sta	su_byte_count

		ldy	#$00
@cell
		lda	(src_ptr),y
		sta	(dst_ptr),y
		iny
		cpy	su_byte_count
		bne	@cell

; advance dst_ptr by su_byte_count
		tya
		clc
		adc	dst_ptr
		sta	dst_ptr
		bcc	@dst_ok
		inc	dst_ptr+1
@dst_ok

; advance src_ptr by 160 (full row stride)
		lda	src_ptr
		clc
		adc	#160
		sta	src_ptr
		bcc	@src_ok
		inc	src_ptr+1
@src_ok

		dec	su_rows_left
		bne	@row
		rts
.endproc


.proc restore_under_rect
; Inverse of save_under_rect — copies save_under_buf back to (menu_row, menu_col).
		lda	menu_row
		ldx	menu_col
		jsr	menu_compute_addr	; → src_ptr (used as DEST here)

		lda	#<save_under_buf
		sta	dst_ptr
		lda	#>save_under_buf
		sta	dst_ptr+1

		lda	menu_h
		sta	su_rows_left

@row
		lda	menu_w
		asl
		sta	su_byte_count

		ldy	#$00
@cell
		lda	(dst_ptr),y		; dst_ptr = save buf (the "source" of restored bytes)
		sta	(src_ptr),y		; src_ptr = screen (the destination)
		iny
		cpy	su_byte_count
		bne	@cell

		tya
		clc
		adc	dst_ptr
		sta	dst_ptr
		bcc	@dst_ok
		inc	dst_ptr+1
@dst_ok

		lda	src_ptr
		clc
		adc	#160
		sta	src_ptr
		bcc	@src_ok
		inc	src_ptr+1
@src_ok

		dec	su_rows_left
		bne	@row
		rts
.endproc


.proc menu_compute_addr
; A = row, X = col → src_ptr = vbxe_screen_top + row*160 + col*2
		sta	mc_row
		stx	mc_col
		lda	#<vbxe_screen_top
		sta	src_ptr
		lda	#>vbxe_screen_top
		sta	src_ptr+1
		ldx	mc_row
		beq	@col_only
@row_loop
		lda	src_ptr
		clc
		adc	#160
		sta	src_ptr
		bcc	@no_carry
		inc	src_ptr+1
@no_carry
		dex
		bne	@row_loop
@col_only
		lda	mc_col
		asl
		clc
		adc	src_ptr
		sta	src_ptr
		bcc	@done
		inc	src_ptr+1
@done
		rts
.endproc


.proc menu_draw
; Draw the box border, then all items.
		jsr	menu_draw_box
		jsr	menu_redraw_items
		rts
.endproc

.proc menu_redraw_items
; Redraw all menu items at their current positions, applying the highlight
; to menu_selected. Cheap enough to redraw all rows on each highlight move.
		lda	#$00
		sta	mr_idx
@item_loop
		lda	mr_idx
		cmp	menu_count
		bcc	@have_item		; out-of-range to branch to @done directly
		jmp	@done
@have_item

; compute row of this item: menu_row + 1 (for top border) + idx
		lda	menu_row
		clc
		adc	#$01
		clc
		adc	mr_idx
		sta	mr_row

; compute label_ptr from menu_ptr + 5 + idx*4
		lda	mr_idx
		asl
		asl
		clc
		adc	#5
		tay
		lda	(menu_ptr),y
		sta	mr_label
		iny
		lda	(menu_ptr),y
		sta	mr_label+1

; choose color: highlight if idx == menu_selected, but never highlight a
; non-selectable row — the cursor cannot land on one anyway
		lda	mr_idx
		jsr	menu_get_action
		beq	@normal
		lda	mr_idx
		cmp	menu_selected
		beq	@hilite
@normal
		lda	#MENU_COLOR_NORM
		jmp	@have_color
@hilite
		lda	#MENU_COLOR_HILT
@have_color
		sta	mr_color

; clear interior of this row first (col+1 .. col+w-2) with spaces
		lda	#' '
		jsr	to_glyph		; hoisted out of the loop
		sta	mr_blank
		lda	mr_row
		ldx	menu_col
		inx				; skip left border
		jsr	menu_compute_addr	; → src_ptr points to first interior cell
		ldy	#$00
		lda	menu_w
		sec
		sbc	#$02			; interior width = w - 2
		sta	mr_inner_w
@clear_loop
		lda	mr_blank
		sta	(src_ptr),y
		iny
		lda	mr_color
		sta	(src_ptr),y
		iny
		dec	mr_inner_w
		bne	@clear_loop

; mark the loaded font with a '*' in the pad column, so the menu says which of
; the font rows is actually live
        lda menu_ptr
        cmp #<main_menu
        bne @no_marker
        lda menu_ptr+1
        cmp #>main_menu
        bne @no_marker
		lda	mr_idx
		cmp	active_font
		bne	@no_marker
		lda	mr_row
		ldx	menu_col
		inx
		jsr	menu_compute_addr
		lda	#'*'
		jsr	to_glyph
		ldy	#$00
		sta	(src_ptr),y
		iny
		lda	mr_color
		sta	(src_ptr),y
@no_marker

; draw the label at col+2 (one space of padding inside the box)
		lda	mr_row
		ldx	menu_col
		inx
		inx				; left border + 1 pad
		stx	mr_putcol
		lda	mr_label
		ldx	mr_label+1
		ldy	mr_color
		jsr	menu_put_str_at		; (label A/X, row→mr_row, col→mr_putcol, color Y)

		inc	mr_idx
		jmp	@item_loop
@done
        jmp plus_dirty_all
.endproc


.proc box_glyph
; Y = BOX_* slot → A = the glyph index for that slot in the active font.
; Clobbers A only.
		lda	atascii_mode
		bne	@atascii
		lda	box_glyphs_cp437,y
		rts
@atascii	lda	box_glyphs_atascii,y
		rts
.endproc


.proc menu_draw_box
; Draw the box border, in whichever glyph set the active font provides.
		lda	menu_h
		sta	mb_h_left

		lda	menu_row
		sta	mb_cur_row

@row_loop
		lda	mb_cur_row
		ldx	menu_col
		jsr	menu_compute_addr	; → src_ptr at left edge of this row

; decide what kind of row this is: top, bottom, or middle
		lda	mb_cur_row
		cmp	menu_row
		beq	@is_top
		lda	mb_h_left
		cmp	#$01
		beq	@is_bottom
		jmp	@is_middle

@is_top
		ldy	#BOX_UL
		jsr	box_glyph
		sta	mb_corner
		ldy	#BOX_UR
		jsr	box_glyph
		sta	mb_corner_r
		ldy	#BOX_HORZ
		jsr	box_glyph
		sta	mb_fill
		jmp	@draw

@is_bottom
		ldy	#BOX_LL
		jsr	box_glyph
		sta	mb_corner
		ldy	#BOX_LR
		jsr	box_glyph
		sta	mb_corner_r
		ldy	#BOX_HORZ
		jsr	box_glyph
		sta	mb_fill
		jmp	@draw

@is_middle
		ldy	#BOX_VERT
		jsr	box_glyph
		sta	mb_corner
		sta	mb_corner_r
		ldy	#BOX_SPACE
		jsr	box_glyph
		sta	mb_fill
		; fall through

@draw
		ldy	#$00
		lda	menu_w
		sta	mb_w_left

; left edge cell
		lda	mb_corner
		sta	(src_ptr),y
		iny
		lda	#MENU_COLOR_NORM
		sta	(src_ptr),y
		iny
		dec	mb_w_left

; middle cells (w - 2 of them)
		lda	mb_w_left
		cmp	#$01
		bcc	@right_edge
@mid_loop
		lda	mb_w_left
		cmp	#$01
		beq	@right_edge
		lda	mb_fill
		sta	(src_ptr),y
		iny
		lda	#MENU_COLOR_NORM
		sta	(src_ptr),y
		iny
		dec	mb_w_left
		jmp	@mid_loop

@right_edge
; right edge cell
		lda	mb_corner_r
		sta	(src_ptr),y
		iny
		lda	#MENU_COLOR_NORM
		sta	(src_ptr),y

		inc	mb_cur_row
		dec	mb_h_left
		beq	@all_done		; out-of-range to bne back; invert + jmp
		jmp	@row_loop
@all_done
		rts
.endproc


.proc menu_put_str_at
; A/X = pointer to null-terminated string, mr_row = row, mr_putcol = col,
; Y on entry = color attribute byte.  Writes char + color cells to the
; overlay.  src_ptr is advanced one cell (+2) per character; ps_str one
; byte per character.  Y is held at 0 throughout for indirect-Y reads.
		sta	ps_str
		stx	ps_str+1
		sty	ps_color

		lda	mr_row
		ldx	mr_putcol
		jsr	menu_compute_addr	; → src_ptr (destination)

@loop
		ldy	#$00
		lda	(ps_str),y
		beq	@done
		jsr	to_glyph		; labels are ASCII, and ASCII and ATASCII agree
		ldy	#$00			; over $20-$7A — but to_glyph clobbers Y
		sta	(src_ptr),y		; char into cell low byte
		iny
		lda	ps_color
		sta	(src_ptr),y		; color into cell high byte

; advance src_ptr by 2 (next cell)
		clc
		lda	src_ptr
		adc	#$02
		sta	src_ptr
		bcc	@no_dst_carry
		inc	src_ptr+1
@no_dst_carry

; advance ps_str by 1 (next char)
		inc	ps_str
		bne	@loop
		inc	ps_str+1
		jmp	@loop
@done
		rts
.endproc

; --- menu actions (leaf — set menu_dismiss to close on completion) ---
;
; font_load_index loads a font from disk via _vbxe_load_font (IOCB 3), bracketed
; by font_swap_prep_r / font_swap_restore_r. The disk SIO inside _vbxe_load_font
; triggers the OS SIOInitHardware path, which clobbers POKEY's serial-port config
; (AUDCTL, AUDF3/AUDF4 baud divisors, SKCTL) and clears POKMSK serial-IRQ
; bits 4-5 — fine for N: (request/response SIO), fatal for R: which holds
; POKEY in concurrent-I/O mode. font_swap_restore_r re-issues the R: configure
; XIO sequence to revive POKEY when device_type == 0.
;
; That bracketing belongs to font_load_index alone. Callers must not add a
; restore of their own — one CLOSE must pair with exactly one OPEN.

.proc font_swap_prep_r
; Phase 7: pre-disk-SIO CLOSE on IOCB 1 when connected via R:. Lets the R:
; handler cleanly tear down concurrent mode (NS_EndConcurrent pattern:
; disable serial IRQs, restore VSER* vectors, deassert motor line) while it
; is still healthy. Phase 6 attempted CLOSE *after* the disk SIO when the
; handler was already wedged and hung in CIOV — here we act before that
; happens. No-op on N:, which is unaffected by disk SIO interleaving.
		lda	dev_ready
		beq	@done			; no device open — skip the R: CLOSE entirely
		lda	device_type
		bne	@done
		ldx	#$10
		lda	#$0C			; CMD_CLOSE
		sta	ICCOM+$10
		jsr	CIOV
@done		rts
.endproc

.proc font_swap_restore_r
; Post-disk-SIO recovery. On R:, re-OPEN IOCB 1 via open_r_device (which
; falls through to configure_r_device for XIO 36/38/34/40). The R: handler
; goes through its full open + concurrent-mode setup, same as at boot.
; N: needs no recovery — never closed.
		lda	dev_ready
		beq	@done			; no device open — no R: open/reconfigure needed
		lda	device_type
		bne	@done
		jsr	open_r_device
@done		rts
.endproc

.proc font_load_index
; A = main_menu item index of a font row → load that font into VBXE font RAM.
; Bracketed by the R: close/reopen because _vbxe_load_font does disk SIO.
; Does NOT dismiss the menu — the caller decides that.
		sec
		sbc	#MENU_ITEM_FONT_BASE
		asl				; * 2 for the pointer table
		tax
		lda	font_path_table,x
		sta	fl_path			; park the path across font_swap_prep_r
		lda	font_path_table+1,x
		sta	fl_path+1
		jsr	font_swap_prep_r
		lda	fl_path
		ldx	fl_path+1
		jsr	_vbxe_load_font
        jsr plus_font_changed
		jmp	font_swap_restore_r
.endproc

.proc font_load_selected
; The action behind every font row. The row's own position in main_menu selects
; the path, so adding a font is one label, one path and one menu line — no new
; action proc.
;
; Picking a CP437 font also means picking ANSI: the two can never disagree,
; which is what makes the single Mode row sufficient for mutual exclusion.
		lda	menu_selected
		sta	active_font
		jsr	font_load_index
		lda	#$00
		sta	atascii_mode
		lda	#79
		sta	term_last_col
        jsr plus_font_changed
		jsr	menu_init_labels	; the Mode row may have just changed

; Dismiss directly rather than via a restore helper: font_load_index has already
; re-opened R: on the way out. Going through a second font_swap_restore_r here
; issued a second OPEN on an IOCB 1 that was still open, and the XL OS OPEN path
; at $E597 skips the HATABS device lookup when ICHID != $FF — so instead of
; erroring it re-entered the 850 handler's OPEN on a live port (zeroing its
; per-port state) and re-ran the whole XIO 36/38/34/40 sequence, including
; XIO 40 on a port already in concurrent mode.
		lda	#$01
		sta	menu_dismiss
		rts
.endproc

.proc act_cycle_mode
; RETURN on the Mode row. Each terminal mode owns its character set, so this
; also swaps the font and the default wrap width. The menu deliberately stays
; open (no menu_dismiss) so the effect is visible against the menu itself.
		lda	atascii_mode
		eor	#$01
		sta	atascii_mode
		beq	@to_ansi

; ATASCII: build the charset from the OS ROM. No CIO and no SIO, so unlike the
; font rows this needs no R: close/reopen bracketing at all.
		jsr	_vbxe_font_from_rom
        jsr plus_font_changed
		lda	#39			; ATASCII art is drawn for a 40-column screen
		sta	term_last_col
		jmp	@refresh

; ANSI: reload whichever font row is marked active — its glyphs were overwritten
; by the ROM charset.
@to_ansi	lda	#79
		sta	term_last_col
		lda	active_font
		jsr	font_load_index

; Full redraw, not just the items: the character set changed underneath us, so
; the box border's glyph codes have to be re-emitted from the other glyph set.
@refresh jsr plus_font_changed
        jsr menu_init_labels
		jmp	plus_recenter_menu
.endproc

.proc act_cycle_width
; RETURN on the Width row toggles the auto-wrap column between 80 and 40.
; Independent of mode, so ATASCII can be forced to 80 for hosts that assume it.
		lda	term_last_col
		cmp	#79
		beq	@to_40
		lda	#79
		bne	@store			; always taken
@to_40		lda	#39
@store		sta	term_last_col
        jsr plus_enable_bitmap
        jsr telnet_naws
		jsr	menu_init_labels
		jmp	plus_recenter_menu
.endproc

.proc menu_init_labels
; Rebuild the two value rows from live terminal state. Called at startup and by
; any action that changes mode or width, so the labels are derived data rather
; than something that can drift out of sync.
		lda	#<lbl_mode
		sta	dst_ptr
		lda	#>lbl_mode
		sta	dst_ptr+1
		lda	atascii_mode
		beq	@mode_ansi
		lda	#<str_mode_atascii
		ldx	#>str_mode_atascii
		jsr	copy_to_label
		jmp	@width
@mode_ansi	lda	#<str_mode_ansi
		ldx	#>str_mode_ansi
		jsr	copy_to_label

@width		lda	#<lbl_width
		sta	dst_ptr
		lda	#>lbl_width
		sta	dst_ptr+1
		lda	term_last_col
		cmp	#79
		beq	@width_80
		lda	#<str_width_40
		ldx	#>str_width_40
		jmp	copy_to_label
@width_80	lda	#<str_width_80
		ldx	#>str_width_80
		jmp	copy_to_label
.endproc

.proc copy_to_label
; A/X = null-terminated source, dst_ptr = destination buffer. src_ptr/dst_ptr are
; safe here: the only callers are menu actions and init_terminal_state, neither
; of which runs inside print_str or read_line_vbxe.
		sta	src_ptr
		stx	src_ptr+1
		ldy	#$00
@loop		lda	(src_ptr),y
		sta	(dst_ptr),y
		beq	@done
		iny
		cpy	#MENU_LABEL_MAX-1
		bcc	@loop
		lda	#$00			; hard-terminate an over-long string
		sta	(dst_ptr),y
@done		rts
.endproc

; --- menu data ---
;
; Still a flat menu; submenus would require either per-menu save buffers or
; a stack-style allocator on save_under_buf to support nesting safely.
;
; Two kinds of row now exist:
;   * command rows — RETURN calls action_ptr (font rows, which then dismiss)
;   * value rows   — RETURN calls action_ptr, which mutates state, rewrites its
;                    own label and redraws, leaving the menu open
;   * an action_ptr of $0000 marks a non-selectable divider or heading: the
;     highlight steps over it and RETURN ignores it
;
; To add a font: one label, one entry in font_path_table, one menu line, and
; bump the count + height. To add a setting: a value row and its action.

MENU_ITEM_FONT_BASE = 3				; main_menu index of the first font row
MENU_ITEM_ATARI	= MENU_ITEM_FONT_BASE + 1	; AtariPC — the font loaded at boot
MENU_LABEL_MAX	= 16				; size of the mutable label buffers

main_menu
		.byte	3, 30, 20, 18, 16	; row, col, width, height, item count
		.word	lbl_mode,       act_cycle_mode
		.word	lbl_width,      act_cycle_width
		.word	lbl_divider,    0
		.word	lbl_ibm,        font_load_selected
		.word	lbl_atari,      font_load_selected
		.word	lbl_ascprin,    font_load_selected
		.word	lbl_balloon,    font_load_selected
		.word	lbl_bozo,       font_load_selected
		.word	lbl_bzzz2,      font_load_selected
		.word	lbl_casualgt,   font_load_selected
		.word	lbl_computer,   font_load_selected
		.word	lbl_cursive,    font_load_selected
		.word	lbl_hero,       font_load_selected
		.word	lbl_newsletter, font_load_selected
		.word	lbl_preppie,    font_load_selected
		.word	lbl_shadow,     font_load_selected

; Font paths, in main_menu row order — index = item index - MENU_ITEM_FONT_BASE.
font_path_table
		.word	font_path_ibm
		.word	font_path_atari
		.word	font_path_ascprin
		.word	font_path_balloon
		.word	font_path_bozo
		.word	font_path_bzzz2
		.word	font_path_casualgt
		.word	font_path_computer
		.word	font_path_cursive
		.word	font_path_hero
		.word	font_path_newsletter
		.word	font_path_preppie
		.word	font_path_shadow

; Value-row text. menu_init_labels copies one of each pair into the mutable
; label buffers; the buffers, not these, are what main_menu points at.
str_mode_ansi	.byte	"Mode:  ANSI", 0
str_mode_atascii .byte	"Mode:  ATASCII", 0
str_width_80	.byte	"Width: 80", 0
str_width_40	.byte	"Width: 40", 0

; Plain hyphens rather than a box-drawing rule: menu_put_str_at runs labels
; through to_glyph, and '-' is the one horizontal mark that survives that
; translation intact in both character sets.
lbl_divider	.byte	"----------------", 0

lbl_ibm		.byte	"IBMPC", 0
lbl_atari	.byte	"AtariPC", 0
lbl_ascprin	.byte	"AscPrint", 0
lbl_balloon	.byte	"Balloon", 0
lbl_bozo	.byte	"Bozo", 0
lbl_bzzz2	.byte	"Bzzz2", 0
lbl_casualgt	.byte	"Casual GT", 0
lbl_computer	.byte	"Computer", 0
lbl_cursive	.byte	"Cursive", 0
lbl_hero	.byte	"Hero", 0
lbl_newsletter	.byte	"Newsletter", 0
lbl_preppie	.byte	"Preppie", 0
lbl_shadow	.byte	"Shadow", 0

font_path_ibm		.byte	"D:IBMPC.FNT", $9B
font_path_atari		.byte	"D:ATARIPC.FNT", $9B
font_path_ascprin	.byte	"D:ASCPRINP.FNT", $9B
font_path_balloon	.byte	"D:BALOONPC.FNT", $9B
font_path_bozo		.byte	"D:BOZOPC.FNT", $9B
font_path_bzzz2		.byte	"D:BZZZ2PC.FNT", $9B
font_path_casualgt	.byte	"D:CASUALGT.FNT", $9B
font_path_computer	.byte	"D:COMPUTPC.FNT", $9B
font_path_cursive	.byte	"D:CURSIVEP.FNT", $9B
font_path_hero		.byte	"D:HEROPC.FNT", $9B
font_path_newsletter	.byte	"D:NEWSLTRP.FNT", $9B
font_path_preppie	.byte	"D:PREPPIEP.FNT", $9B
font_path_shadow	.byte	"D:SHADOWPC.FNT", $9B

; --- BSS-style scratch (zero-initialised at load) ---

sr_top		.res	1		; scroll_rgn_* effective top row
sr_bot		.res	1		; scroll_rgn_* effective bottom row
sr_n		.res	1		; scroll_rgn_* line count
sr_rows		.res	1		; rows remaining in the row-move loop
rc_last		.res	1		; row_*_char: byte offset of the last writable cell
rc_floor	.res	1		; row_*_char: byte offset of the cursor cell
fl_path		.res	2		; font_load_index: path pointer held across the R: close

; Mutable menu labels for the value rows — these hold text, not state, and are
; regenerated from terminal state by menu_init_labels.
lbl_mode	.res	MENU_LABEL_MAX
lbl_width	.res	MENU_LABEL_MAX
su_rows_left	.res	1
su_byte_count	.res	1
mc_row		.res	1
mc_col		.res	1
mr_idx		.res	1
mr_row		.res	1
mr_label	.res	2
mr_color	.res	1
mr_blank	.res	1		; space glyph for the active font, hoisted per row
mr_inner_w	.res	1
mr_putcol	.res	1
mb_h_left	.res	1
mb_w_left	.res	1
mb_cur_row	.res	1
mb_corner	.res	1		; left-edge / left-corner glyph for current row
mb_corner_r	.res	1		; right-edge / right-corner glyph for current row
mb_fill		.res	1
ps_color	.res	1
save_under_buf	.res	1408		; menu_w × menu_h × 2 bytes; main_menu needs 20×18×2 = 720

;###################################################################################################################

.proc open_r_device
; open and fully configure the R: serial device on IOCB 1.

; open
		ldx	#$10
		lda	#$03
		sta	ICCOM+$10
		lda	#<r_path
		sta	ICBA+$10
		lda	#>r_path
		sta	ICBA+$11
		lda	#$0D
		sta	ICAX1+$10
		lda	#0
		sta	ICAX2+$10
		jsr	CIOV

; fall through to configure_r_device for baud / translation / DTR / concurrent
.endproc

.proc configure_r_device
; Re-issue the XIO settings needed to put R: into a known concurrent-I/O state.
; Called once after OPEN at startup, AND again after any disk-driven CIO
; (e.g. font load) — Atari OS SIO clobbers POKEY's serial port config (AUDCTL,
; AUDF3/AUDF4, SKCTL) and clears POKMSK serial-IRQ bits 4-5 on every disk
; transaction, leaving R: handler with the wrong baud divisors and disabled
; serial IRQs.  Re-running these XIO commands re-applies the R: handler's
; POKEY init via XIO 40, restoring R: to working order.
;
; This only works because every caller reaches us by falling through
; open_r_device, i.e. immediately after an OPEN, with concurrent mode not yet
; running.  XIO 40 is not idempotent: the handler's start-concurrent routine
; begins `LDA $23F7 / BEQ ... / LDY #$99 / RTS`, so issued while concurrent mode
; is already active it returns error $99 and re-applies nothing.  Do not call
; this expecting to revive a live R: session in place — close and reopen.
;
; AUX1 = 0 on the XIO 40 below is what selects the handler's own 32-byte input
; buffer at $23FA: the buffer choice is gated on AUX1 ($21A2: LDA ICAX1Z / BNE),
; not on ICBL.  ICBL is only read when AUX1 is non-zero, so leaving it holding
; a stale length from a previous GET/PUT_CHARS is harmless here.

; 9600 baud, 8 data bits, no status line checking
		ldx	#$10
		lda	#36
		sta	ICCOM+$10
		lda	#<r_path
		sta	ICBA+$10
		lda	#>r_path
		sta	ICBA+$11
		lda	serial_baud_code
		sta	ICAX1+$10
		lda	#0
		sta	ICAX2+$10
		jsr	CIOV

; no translation, ignore parity, no LF append
		ldx	#$10
		lda	#38
		sta	ICCOM+$10
		lda	#<r_path
		sta	ICBA+$10
		lda	#>r_path
		sta	ICBA+$11
		lda	#32
		sta	ICAX1+$10
		lda	#0
		sta	ICAX2+$10
		jsr	CIOV

; turn on DTR and RTS
		ldx	#$10
		lda	#34
		sta	ICCOM+$10
		lda	#<r_path
		sta	ICBA+$10
		lda	#>r_path
		sta	ICBA+$11
		lda	#192+48
		sta	ICAX1+$10
		lda	#0
		sta	ICAX2+$10
		jsr	CIOV

; start concurrent I/O — re-applies POKEY init (AUDCTL, SKCTL, IRQEN bits 4-5)
		ldx	#$10
		lda	#40
		sta	ICCOM+$10
		lda	#<r_path
		sta	ICBA+$10
		lda	#>r_path
		sta	ICBA+$11
		lda	#0
		sta	ICAX1+$10
		sta	ICAX2+$10
		jmp	CIOV
.endproc

font_path	.byte	"D:ATARIPC.FNT", $9B
pallette_path	.byte	"D:ANSI.PAL", $9B
;test_file	.byte	"D:TEST.ANS", $9B
r_path		.byte	"R1:", $9B
.segment "BSS"
n_url_buf .res 256              ; Shared with banked UI; must remain above $8000.
.segment "CODE"

.proc open_n_device
; Open a FujiNet connection via direct SIO (no N: CIO handler required).
; URL is in n_url_buf.  Unit number is parsed from the URL (N1:, N2:, etc.;
; bare N: defaults to unit 1).  Returns Y=$01 (positive) on success,
; Y=SIO error (negative) on failure.  Also saves unit in n_unit for recv/send.

; Parse unit number: n_url_buf[1] is ':' for bare N:, or digit for N1:-N8:
		ldy	#$01
		lda	n_url_buf,y
		cmp	#':'
		bne	has_digit
		lda	#$01			; bare N: → unit 1
		bne	store_unit
has_digit	sec
		sbc	#'0'
store_unit	sta	n_unit

; SIO OPEN: send URL to FujiNet device $71
		lda	#FUJI_ID
		sta	DDEVIC
		lda	n_unit
		sta	DUNIT
		lda	#'O'			; Open command
		sta	DCOMND
		lda	#$80			; write direction (sending URL spec)
		sta	DSTATS
		lda	#<n_url_buf
		sta	DBUFLO
		lda	#>n_url_buf
		sta	DBUFHI
		lda	#FUJI_TIMEOUT
		sta	DTIMLO
		lda	#$00
		sta	DTIMHI
		lda	#$00			; 256 bytes (FujiNet OPEN always expects 256-byte URL buffer)
		sta	DBYTLO
		lda	#$01
		sta	DBYTHI
		lda	#$0C			; OUPDATE (read+write)
		sta	DAUX1
		lda	#$00			; no translation
		sta	DAUX2
		jsr	SIOV

; SIOV stores result in DSTATS: $01=success, negative=error
		lda	DSTATS
		cmp	#$01
		beq	ok
		cmp	#$90			; DERROR — call STATUS to get extended error and clean up
		bne	not_derror
		jsr	n_status_sio
		lda	#$90			; return DERROR ($90, bit 7 set → bmi taken)
not_derror	tay
		rts
ok		ldy	#$01			; positive Y = success
		rts
.endproc

.proc n_status_sio
; Call FujiNet STATUS SIO command. Fills DVSTAT0–DVSTAT3. Requires n_unit set.
		lda	#FUJI_ID
		sta	DDEVIC
		lda	n_unit
		sta	DUNIT
		lda	#'S'
		sta	DCOMND
		lda	#$40			; DREAD
		sta	DSTATS
		lda	#<DVSTAT0
		sta	DBUFLO
		lda	#>DVSTAT0
		sta	DBUFHI
		lda	#FUJI_TIMEOUT
		sta	DTIMLO
		lda	#$00
		sta	DTIMHI
		lda	#$04
		sta	DBYTLO
		lda	#$00
		sta	DBYTHI
		sta	DAUX1
		sta	DAUX2
		jmp	SIOV
.endproc

.proc nlogin_n_device
; Pre-configure SSH credentials via SIO commands $FD (login) and $FE (password).
; Called before open_n_device. Errors ignored — non-SSH connections will reject gracefully.
; Requires n_unit set.
		lda	#FUJI_ID
		sta	DDEVIC
		lda	n_unit
		sta	DUNIT
		lda	#$FD			; login command
		sta	DCOMND
		lda	#$80			; DWRITE
		sta	DSTATS
		lda	#<login_buf
		sta	DBUFLO
		lda	#>login_buf
		sta	DBUFHI
		lda	#FUJI_TIMEOUT
		sta	DTIMLO
		lda	#$00			; 256-byte buffer (hi byte = 1)
		sta	DTIMHI
		sta	DBYTLO
		lda	#$01
		sta	DBYTHI
		lda	#$00
		sta	DAUX1
		sta	DAUX2
		jsr	SIOV			; send username
		lda	#$FE			; password command (reuse most params)
		sta	DCOMND
		lda	#$80			; DWRITE — SIOV overwrote DSTATS with result code
		sta	DSTATS
		lda	#<password_buf
		sta	DBUFLO
		lda	#>password_buf
		sta	DBUFHI
		jsr	SIOV			; send password
		rts
.endproc

.proc nclose_n_device
; Send FujiNet CLOSE SIO command. Requires n_unit set.
		lda	#FUJI_ID
		sta	DDEVIC
		lda	n_unit
		sta	DUNIT
		lda	#'C'
		sta	DCOMND
		lda	#$00
		sta	DSTATS
		sta	DBUFLO
		sta	DBUFHI
		lda	#FUJI_TIMEOUT
		sta	DTIMLO
		lda	#$00
		sta	DTIMHI
		sta	DBYTLO
		sta	DBYTHI
		sta	DAUX1
		sta	DAUX2
		jmp	SIOV
.endproc

.proc n_proceed_irq
; VPRCED interrupt handler — set n_trip=1 when FujiNet PROCEED line goes high.
; PLA+RTI matches the Atari OS convention: OS pushes A before jumping through VPRCED.
		lda	#$01
		sta	n_trip
		pla
		rti
.endproc

.proc restore_os_hooks
; Restore IRQ/vector state that may have been modified while terminal was active.
		lda	old_dosini
		sta	DOSINI
		lda	old_dosini+1
		sta	DOSINI+1

		lda	PACTL
		and	#$FE
		sta	PACTL			; disable PROCEED IRQ before touching VPRCED

		lda	n_old_vprced
		sta	VPRCED
		lda	n_old_vprced+1
		sta	VPRCED+1

		lda	n_old_pactl
		and	#$FE
		sta	PACTL			; restore PACTL state but keep PROCEED IRQ disabled

		sei
		lda	old_vkeybd
		sta	VKEYBD
		lda	old_vkeybd+1
		sta	VKEYBD+1
		cli

		ldx	#$10
		lda	#$0C			; close IOCB 1 (R:/N:) if open
		sta	ICCOM+$10
		jsr	CIOV

		lda	saved_soundr		; restore OS SIO bus sound setting
		sta	SOUNDR

		lda	saved_portb		; put the BASIC ROM back the way we found it
		sta	PORTB
		rts
.endproc

.proc reset_cleanup
; RESET warmstart hook: restore ANTIC/VBXE-visible state before DOS warm init.
		lda	old_dosini
		sta	DOSINI
		lda	old_dosini+1
		sta	DOSINI+1

		lda	#$00
		sta	video_control
		sta	memac_bank_sel
		sta	memac_control

		lda	saved_sdmctl
		sta	SDMCTL
		sta	$D400

		lda	n_old_vprced
		sta	VPRCED
		lda	n_old_vprced+1
		sta	VPRCED+1

		lda	n_old_pactl
		and	#$FE
		sta	PACTL

		sei
		lda	old_vkeybd
		sta	VKEYBD
		lda	old_vkeybd+1
		sta	VKEYBD+1
		cli

		lda	saved_soundr
		sta	SOUNDR

		jmp	(old_dosini)
.endproc

.proc restore_graphics
		lda	#$00
		sta	SDMCTL				; blank ANTIC immediately during teardown
		sta	$D400				; keep shadow/hardware DMACTL in sync

		jsr	_vbxe_shutdown

		; _vbxe_shutdown already restored SDMCTL from startup state.
		; Mirror that restored shadow value to hardware DMACTL immediately.
		lda	SDMCTL
		sta	$D400

		lda	#$00
		sta	$D01B				; PRIOR (hardware): normal GTIA priority mode
		sta	$026F				; GPRIOR shadow

		lda	#$C0
		sta	$D40E				; NMIEN: enable VBI + DLI (OS default-compatible)

		lda	#$00

		sta	$D016				; COLPF0
		sta	$D017				; COLPF1
		sta	$D018				; COLPF2
		sta	$D019				; COLPF3
		sta	$D01A				; COLBK
		sta	$02C4				; COLOR0 shadow
		sta	$02C5				; COLOR1 shadow
		sta	$02C6				; COLOR2 shadow
		sta	$02C7				; COLOR3 shadow
		sta	$02C8				; COLOR4 shadow

		; Re-open IOCB 0 to E: in text mode so DOS resumes on a clean console.
		ldx	#$00
		lda	#$0C
		sta	ICCOM
		jsr	CIOV

		; Force a fresh OS text display list and palette via screen handler.
		lda	#$03
		sta	ICCOM
		lda	#<s_device
		sta	ICBA
		lda	#>s_device
		sta	ICBA+1
		lda	#$00				; graphics mode 0
		sta	ICAX1
		sta	ICAX2
		jsr	CIOV

		lda	#$0C
		sta	ICCOM
		jsr	CIOV

		lda	#$03
		sta	ICCOM
		lda	#<e_device
		sta	ICBA
		lda	#>e_device
		sta	ICBA+1
		lda	#$00
		sta	ICAX1
		sta	ICAX2
		jsr	CIOV
		rts
.endproc

exit_to_dos
		jsr	restore_os_hooks
		jsr	restore_graphics
		jmp	(DOSVEC)		; return directly to DOS command processor

send_stage_buf	.res	MAX_SEND_BATCH, $00		; coalesced outbound staging buffer
send_count	.res	1, $00				; bytes staged for the current send
.segment "BSS"
dev_ready .res 1                ; 1 = R:/N: open; also read by banked UI.
.segment "CODE"
banner_msg	.byte	$1B,"[31m","P",$1B,"[33m","r",$1B,"[32m","i",$1B,"[36m","s",$1B,"[34m","m",$1B,"[0m","Term 0.3", $9B
select_prompt	.byte	"R=Serial  N=FujiNet? ", $9B
no_n_msg	.byte	"FujiNet open failed: $", $9B
press_return_msg	.byte	" - Press Return.", $9B
kbd_dev		.byte	"K:", $9B
s_device	.byte	"S:", $9B
e_device	.byte	"E:", $9B
select_buf	.res	4, $00
connecting_msg	.byte	"Connecting...", $9B
n_open_ok_msg	.byte	"Connected.", $9B
disconnected_msg	.byte	"Disconnected.", $9B
proto_prompt	.byte	"[T]elnet [R]aw TCP [S]SH: ", $9B
server_prompt	.byte	"SERVER: ", $9B
port_prompt	.byte	"PORT (Enter for default): ", $9B
user_prompt	.byte	"USER: ", $9B
pw_prompt	.byte	"PASSWORD: ", $9B
n1_str		.byte	"N1:", $9B
tcp_url_str	.byte	"TCP://", $9B
ssh_url_str	.byte	"SSH://", $9B
tcp_port_str	.byte	"23", $9B
ssh_port_str	.byte	"22", $9B
.segment "BSS"
server_buf .res 64
port_buf .res 8
proto_byte .res 1
.segment "CODE"
n_old_vprced	.res	2, $00			; saved VPRCED vector
n_old_pactl	.byte	$00			; saved PACTL state
old_vkeybd	.res	2, $00			; saved VKEYBD vector (OS keyboard IRQ)
old_dosini	.res	2, $00			; saved DOSINI warm-init vector for RESET chaining
saved_soundr	.res	1, $00			; saved SOUNDR (SIO bus sound) value
.segment "BSS"
login_buf .res 256              ; FujiNet $FD expects 256 bytes; UI shares first 64.
.segment "CODE"
password_buf	.res	256, $00		; password buffer (256 bytes — FujiNet $FE expects 256)

;temp_char	.byte	$00			; temporary location for storing a character

xdl						; start of xdl

; displays 24 scanlines of no overlay (ANTIC display list should be displaying blank
; lines of GTIA background color)
;
; The ATT block below picks the palettes: bits 7-6 = playfield/PMG palette, bits 5-4 =
; Overlay palette, bits 1-0 = Overlay width.  The Overlay is on palette 1 (VBXE's own
; default for it) so that palette 0 — the palette every ANTIC/GTIA colour register value
; is rendered through — is left as the stock Atari palette.  Nothing restores palette 0
; on RESET, so an ANSI palette parked there would follow us out of the program: see the
; palette-programming block in vbxe_lib.asm for the full reasoning.

		.byte	%00110100		; OVOFF, MAPOFF, RPTL - overlay off, color map off, repeat scanlines
		.byte	%00001000		; ATT - display size and overlay priority
		.byte	24-1			; 24 scanlines
		.byte	%00010001		; PF palette 0, OV palette 1, ANTIC normal width
		.byte	%11111111		; overlay is priority over everything

; now on to the 80x24 text portion

		.byte	%01100001		; RPTL, OVADR
		.byte	%10000001		; CHBASE, XDL_END
		.byte	192-1			; 192 scanlines of text (24 rows)

; overlay address starts at top of memory - 3840. this means the last line is at the top of memory

		.faraddr $080000 - (80*24*2)	; overlay address starts at the top of VBXE memory - bytes per screen
		.word	80 + 80			; each line of text is 80 characters and 80 colors (80 + 80 bytes)
		.byte	$00			; font is at the beginning of memory (far from text window)
xdl_end

bcb_start					; start of blitter lists

bcb_one_down					; blitter to scroll one line down
		.faraddr $07F1A0		; top of VBXE ram is $80000. this address is 5 pages back, plus one line. (src)
                .word	80*2			; one line is 160 bytes wide, and we're working our way down, so positive
                .byte	1			; x step is 1 (we want forwards and to not skip stuff)
                .faraddr $07F100		; top of VBXE ram is $80000. this address is 5 pages back. (dst)
                .word	80*2			; x and y step is the same as the source ones
                .byte	1
                .word	(80*2)-1		; width same as y step less one
		.byte	24-2		; 5 pages less one line, then minus one because that's what the doc says
		.byte	$FF			; AND mask. don't modify the data
		.byte	$00			; XOR mask. don't modify the data
		.byte	$00			; no collisions
		.byte	$00			; 1:1 zoom
		.byte	$00			; no pattern
		.byte	%00001000		; NOT last entry, copy mode
		
		.faraddr 0			; doesn't matter
		.word	0			; doesn't matter
		.byte	0			; doesn't matter
		.faraddr $7FF60			; one line before the end of the screen
		.word	80*2			; y step really doesn't matter, since we're only doing one line, but it's 80 characters
		.byte	2			; x step is 2, we only want the character bytes, not color (for now)
		.word	80-1			; width is 80 bytes
		.byte	1-1			; just the last line
		.byte	0			; filling with a pattern
		.byte	0			; fill value is 0
		.byte	0			; no collisions
		.byte	0			; 1:1 zoom
		.byte	0			; no pattern
		.byte	%00001000	        ; NOT last entry, copy mode.
		
		.faraddr 0			; doesn't matter
		.word	0			; doesn't matter
		.byte	0			; doesn't matter
		.faraddr $7FF61			; one line before the end of the screen, and one more so we fill the color bytes.
		.word	80*2			; y step really doesn't matter, since we're only doing one line, but it's 80 characters
		.byte	2			; x step is 2, we only want the color bytes, not character
		.word	80-1			; width is 80 bytes
		.byte	1-1			; just the last line
		.byte	0			; filling with a pattern
scroll_1d_color	.byte	0			; fill value will be changed by whatever uses this blitter list
		.byte	0			; no collisions
		.byte	0			; 1:1 zoom
		.byte	0			; no pattern
		.byte	0			; last entry, copy mode.
		
bcb_clr_scr					; clear visible screen only

		.faraddr 0			; doesn't matter
		.word	0			; doesn't matter
		.byte	0			; doesn't matter
		.faraddr $7F100			; one page before the end of the screen
		.word	80*2			; y step is 80 characters
		.byte	2			; x step is 2, we only want the character bytes, not color
		.word	80-1			; width is 80 bytes
		.byte	24-1			; 24 lines
		.byte	0			; filling with a pattern
		.byte	0			; fill value is 0
		.byte	0			; no collisions
		.byte	0			; 1:1 zoom
		.byte	0			; no pattern
		.byte	%00001000		; NOT last entry, copy mode.
		
		.faraddr 0			; doesn't matter
		.word	0			; doesn't matter
		.byte	0			; doesn't matter
		.faraddr $7F101			; one page before the end of the screen, and one more so we fill the color bytes.
		.word	80*2			; y step really doesn't matter, since we're only doing one line, but it's 80 characters
		.byte	2			; x step is 2, we only want the color bytes, not characters
		.word	80-1			; width is 80 bytes
		.byte	24-1			; 24 lines
		.byte	0			; filling with a pattern
clr_scr_color	.byte	0			; fill value will be changed by whatever uses this blitter list
		.byte	0			; no collisions
		.byte	0			; 1:1 zoom
		.byte	0			; no pattern
		.byte	0			; last entry, copy mode.
		
bcb_end

;###################################################################################################################
; table of addresses for control function handlers
escape_seq	.byte	$1B, '[', 'A'		; $80 - cursor up    (CTRL+- on Atari)
		.byte	$1B, '[', 'B'		; $81 - cursor down  (CTRL+= on Atari)
		.byte	$1B, '[', 'C'		; $82 - cursor right (CTRL+* on Atari)
		.byte	$1B, '[', 'D'		; $83 - cursor left  (CTRL++ on Atari)

;###################################################################################################################
; keycode to ASCII table
; zero means 'do nothing' for now
;		.byte	ascii-val		;key number - ascii mapping - atari key

keycode_table	.byte	$6C			;0 - l - l
		.byte	$6A			;1 - j - j
		.byte	$3B			;2 - ; - ;
		.byte	0			;3 - no key
		.byte	0			;4 - no key
		.byte	$6B			;5 - k - k
		.byte	$2B			;6 - + - +
		.byte	$2A			;7 - * - *
		.byte	$6F			;8 - o - o
		.byte	0			;9 - no key
		.byte	$70			;10 - p - p
		.byte	$75			;11 - u - u
		.byte	$0D			;12 - CR - return
		.byte	$69			;13 - i - i
		.byte	$2D			;14 - minus sign - minus sign
		.byte	$3D			;15 - = - =
		.byte	$76			;16 - v - v
		.byte	0			;17 - n/a - help
		.byte	$63			;18 - c - c
		.byte	0			;19 - no key
		.byte	0			;20 - no key
		.byte	$62			;21 - b - b
		.byte	$78			;22 - x - x
		.byte	$7A			;23 - z - z
		.byte	$34			;24 - 4 - 4
		.byte	0			;25 - no key
		.byte	$33			;26 - 3 - 3
		.byte	$36			;27 - 6 - 6
		.byte	$1B			;28 - esc - esc
		.byte	$35			;29 - 5 - 5
		.byte	$32			;30 - 2 - 2
		.byte	$31			;31 - 1 - 1
		.byte	$2C			;32 - , - ,
		.byte	$20			;33 - space - space bar
		.byte	$2E			;34 - . - .
		.byte	$6E			;35 - n - n
		.byte	0			;36 - no key
		.byte	$6D			;37 - m - m
		.byte	$2F			;38 - / - /
		.byte	0			;39 - n/a - inverse/atari
		.byte	$72			;40 - r - r
		.byte	0			;41 - no key
		.byte	$65			;42 - e - e
		.byte	$79			;43 - y - y
		.byte	$9			;44 - HT - tab
		.byte	$74			;45 - t - t
		.byte	$77			;46 - w - w
		.byte	$71			;47 - q - q
		.byte	$39			;48 - 9 - 9
		.byte	0			;49 - no key
		.byte	$30			;50 - 0 - 0
		.byte	$37			;51 - 7 - 7
		.byte	$8			;52 - BS - bk sp
		.byte	$38			;53 - 8 - 8
		.byte	$3C			;54 - < - <
		.byte	$3E			;55 - > - >
		.byte	$66			;56 - f - f
		.byte	$68			;57 - h - h
		.byte	$64			;58 - d - d
		.byte	0			;59 - no key
		.byte	0			;60 - n/a - caps
		.byte	$67			;61 - g - g
		.byte	$73			;62 - s - s
		.byte	$61			;63 - a - a

; from here down, everything is a repeat of the above 63 keys, but plus shift

		.byte	$4C			;64 - L - L
		.byte	$4A			;65 - J - J
		.byte	$3A			;66 - : - :
		.byte	0			;67 - no key
		.byte	0			;68 - no key
		.byte	$4B			;69 - K - K
		.byte	$5C			;70 - \ - \
		.byte	$5E			;71 - ^ - ^
		.byte	$4F			;72 - O - O
		.byte	0			;73 - no key
		.byte	$50			;74 - P - P
		.byte	$55			;75 - U - U
		.byte	$0D			;76 - CR - shift+return
		.byte	$49			;77 - I - I
		.byte	$5F			;78 - _ - _
		.byte	$7C			;79 - | - |
		.byte	$56			;80 - V - V
		.byte	0			;81 - n/a - shift+help
		.byte	$43			;82 - C - C
		.byte	0			;83 - no key
		.byte	0			;84 - no key
		.byte	$42			;85 - B - B
		.byte	$58			;86 - X - X
		.byte	$5A			;87 - Z - Z
		.byte	$24			;88 - $ - $
		.byte	0			;89 - no key
		.byte	$23			;90 - # - #
		.byte	$26			;91 - & - &
		.byte	$1B			;92 - esc - shift+esc
		.byte	$25			;93 - % - %
		.byte	$22			;94 - " - "
		.byte	$21			;95 - ! - !
		.byte	$5B			;96 - [ - [
		.byte	$20			;97 - space - shift+space
		.byte	$5D			;98 - ] - ]
		.byte	$4E			;99 - N - N
		.byte	0			;100 - no key
		.byte	$4D			;101 - M - M
		.byte	$3F			;102 - ? - ?
		.byte	0			;103 - n/a - inverse
		.byte	$52			;104 - R - R
		.byte	0			;105 - no key
		.byte	$45			;106 - E - E
		.byte	$59			;107 - Y - Y
		.byte	$9			;108 - HT - shift+tab
		.byte	$54			;109 - T - T
		.byte	$57			;110 - W - W
		.byte	$51			;111 - Q - Q
		.byte	$28			;112 - ( - (
		.byte	0			;113 - no key
		.byte	$29			;114 - ) - )
		.byte	$27			;115 - ' - '
		.byte	$7F			;116 - DEL - shift+BS
		.byte	$40			;117 - @ - @
		.byte	$0C			;118 - FF - clear
		.byte	0			;119 - n/a - insert
		.byte	$46			;120 - F - F
		.byte	$48			;121 - H - H
		.byte	$44			;122 - D - D
		.byte	0			;123 - no key
		.byte	0			;124 - n/a - caps
		.byte	$47			;125 - G - G
		.byte	$53			;126 - S - S
		.byte	$41			;127 - A - A

; from here down, everything is a repeat of the first 63 keys, but plus control

		.byte	$C			;128 - FF - ctrl+l
		.byte	$A			;129 - LF - ctrl+j
		.byte	0			;130 - n/a - ctrl+;
		.byte	0			;131 - no key
		.byte	0			;132 - no key
		.byte	$B			;133 - VT - ctrl+k
		.byte	$83			;134 - LEFT  - ctrl++ (was FS; FS still on ctrl+shift++)
		.byte	$82			;135 - RIGHT - ctrl+* (was RS; RS still on ctrl+shift+*)
		.byte	$F			;136 - SI - ctrl+o
		.byte	0			;137 - no key
		.byte	$10			;138 - DLE - ctrl+p
		.byte	$15			;139 - NAK - ctrl+u
		.byte	0			;140 - n/a - ctrl+return
		.byte	$9			;141 - HT - ctrl+i
		.byte	$80			;142 - UP    - ctrl+- (was US; US still on ctrl+shift+-)
		.byte	$81			;143 - DOWN  - ctrl+=
		.byte	$16			;144 - SYN - ctrl+v
		.byte	0			;145 - n/a - ctrl+help
		.byte	$3			;146 - ETX - ctrl+c
		.byte	0			;147 - no key
		.byte	0			;148 - no key
		.byte	$2			;149 - STX - ctrl+b
		.byte	$18			;150 - CAN - ctrl+x
		.byte	$1A			;151 - SUB - ctrl+z
		.byte	0			;152 - n/a - ctrl+4
		.byte	0			;153 - no key
		.byte	0			;154 - n/a - ctrl+3
		.byte	0			;155 - n/a - ctrl+6
		.byte	0			;156 - n/a - ctrl+esc
		.byte	0			;157 - n/a - ctrl+5
		.byte	0			;158 - n/a - ctrl+2
		.byte	0			;159 - n/a - ctrl+1
		.byte	$1B			;160 - ESC - ctrl+, (ctrl+[)
		.byte	0			;161 - n/a - ctrl+space
		.byte	$1D			;162 - GS - ctrl+. (ctrl+])
		.byte	$E			;163 - SO - ctrl+n
		.byte	0			;164 - no key
		.byte	$D			;165 - CR - ctrl+m
		.byte	$0F			;166 - SI - ctrl+/ (ctrl+?)
		.byte	0			;167 - n/a - inverse
		.byte	$12			;168 - DC2 - ctrl+r
		.byte	0			;169 - no key
		.byte	$05			;170 - ENQ - ctrl+e
		.byte	$19			;171 - EM - ctrl+y
		.byte	0			;172 - n/a - ctrl+tab
		.byte	$14			;173 - DC4 - ctrl+t
		.byte	$17			;174 - ETB - ctrl+w
		.byte	$11			;175 - DC1 - ctrl+q
		.byte	0			;176 - n/a - ctrl+9
		.byte	0			;177 - no key
		.byte	0			;178 - n/a - ctrl+0
		.byte	0			;179 - n/a - ctrl+7
		.byte	0			;180 - n/a - ctrl+BS
		.byte	0			;181 - n/a - ctrl+8
		.byte	$7B			;182 - { - ctrl+<
		.byte	$7D			;183 - } - ctrl+>
		.byte	$6			;184 - ACK - ctrl+f
		.byte	$8			;185 - BS - ctrl+h
		.byte	$4			;186 - EOT - ctrl+d
		.byte	0			;187 - no key
		.byte	0			;188 - n/a - caps
		.byte	$7			;189 - BEL - ctrl+g
		.byte	$13			;190 - DC3 - ctrl+s
		.byte	$1			;191 - SOH - ctrl+a

; from here down, everything is a repeat of the first 63 keys, but plus control and shift
; some of these key combos do not work due to the matrix design
; these are marked with *

		.byte	$C			;*192 - FF - ctrl+L
		.byte	$A			;*193 - LF - ctrl+J
		.byte	0			;*194 - n/a - ctrl+:
		.byte	0			;*195 - no key
		.byte	0			;*196 - no key
		.byte	$B			;*197 - VT - ctrl+K
		.byte	$1C			;*198 - FS - ctrl+\
		.byte	$1E			;*199 - SO - ctrl+^
		.byte	$F			;200 - SI - ctrl+O
		.byte	0			;201 - no key
		.byte	$10			;202 - DLE - ctrl+P
		.byte	$15			;203 - NAK - ctrl+U
		.byte	0			;204 - n/a - ctrl+shift+return
		.byte	$9			;205 - HT - ctrl+I
		.byte	$1F			;206 - US - ctrl+_
		.byte	0			;207 - n/a - ctrl+|
		.byte	$16			;*208 - SYN - ctrl+V
		.byte	0			;*209 - n/a - ctrl+shift+help
		.byte	$3			;*210 - ETX - ctrl+C
		.byte	0			;*211 - no key
		.byte	0			;*212 - no key
		.byte	$2			;*213 - STX - ctrl+B
		.byte	$18			;*214 - CAN - ctrl+X
		.byte	$1A			;*215 - SUB - ctrl+Z
		.byte	0			;216 - n/a - ctrl+$
		.byte	0			;217 - no key
		.byte	0			;218 - n/a - ctrl+#
		.byte	0			;219 - n/a - ctrl+&
		.byte	0			;220 - n/a - ctrl+shift+esc
		.byte	0			;221 - n/a - ctrl+%
		.byte	0			;222 - n/a - ctrl+"
		.byte	0			;223 - n/a - ctrl+!
		.byte	$1B			;224 - ESC - ctrl+[
		.byte	0			;225 - n/a - ctrl+shift+space
		.byte	$1D			;226 - GS - ctrl+]
		.byte	$E			;227 - SO - ctrl+N
		.byte	0			;228 - no key
		.byte	$D			;229 - CR - ctrl+M
		.byte	$1F			;230 - US - ctrl+?
		.byte	0			;231 - n/a - ctrl+inverse
		.byte	$12			;232 - DC2 - ctrl+R
		.byte	0			;233 - no key
		.byte	$05			;234 - ENQ - ctrl+E
		.byte	$19			;235 - EM - ctrl+Y
		.byte	0			;236 - n/a - ctrl+shift+tab
		.byte	$14			;237 - DC4 - ctrl+T
		.byte	$17			;238 - ETB - ctrl+W
		.byte	$11			;239 - DC1 - ctrl+Q
		.byte	0			;240 - n/a - ctrl+(
		.byte	0			;241 - no key
		.byte	0			;242 - n/a - ctrl+)
		.byte	0			;243 - n/a - ctrl+'
		.byte	0			;244 - n/a - ctrl+shift+BS
		.byte	0			;245 - n/a - ctrl+@ (no way to send a null, but this is null)
		.byte	0			;246 - n/a - ctrl+shift+clear
		.byte	0			;247 - n/a - ctrl+shift+insert (no function yet)
		.byte	$6			;248 - ACK - ctrl+F
		.byte	$8			;249 - BS - ctrl+H
		.byte	$4			;250 - EOT - ctrl+D
		.byte	0			;251 - no key
		.byte	0			;252 - n/a - caps
		.byte	$7			;253 - BEL - ctrl+G
		.byte	$13			;254 - DC3 - ctrl+S
		.byte	$1			;255 - SOH - ctrl+A

; Version number field
version		.byte	"PrismTerm 0.3"

        .include "plus_core.inc"
        .include "plus_video.inc"
        .include "plus_session.inc"
        .include "plus_telnet.inc"
end
        .segment "BSS"
recv_buffer: .res 256
send_buffer: .res 256
ctrl_seq_buf: .res 256
        .assert * < $A000, lderror, "Program buffers collide with VBXE window"
        .segment "CODE"


.proc plus_bss_init
 lda #<__BSS_RUN__
 sta src_ptr
 lda #>__BSS_RUN__
 sta src_ptr+1
 lda #<__BSS_SIZE__
 sta counter
 lda #>__BSS_SIZE__
 sta counter+1
 ldy #0
@clear: lda counter
 ora counter+1
 beq @out
 lda #0
 sta (src_ptr),y
 inc src_ptr
 bne :+
 inc src_ptr+1
: lda counter
 bne :+
 dec counter+1
: dec counter
 jmp @clear
@out: rts
.endproc

.include "plus_unicode.inc"
.include "plus_bank.inc"
.include "plus_ui_host.inc"
.segment "PROTOCOL"
.org $4000
.include "protocols/kermit.inc"
.include "protocols/zmodem.inc"

.include "protocols/unicode.inc"

.segment "UI"
.org $5000
.include "ui/menu.inc"
