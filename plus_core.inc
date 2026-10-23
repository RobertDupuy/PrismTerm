; PrismTerm — bounded parser, DEC/ANSI operations and fast text path.
; Original authors' license in license.txt applies. Extensions: OpenAI Codex.

.segment "BSS"
plus_state_start:
batch_mode: .res 1
cursor_hidden: .res 1
bell_ticks: .res 1
last_tick: .res 1
wrap_pending: .res 1
wrap_disabled: .res 1
origin_mode: .res 1
insert_mode: .res 1
app_cursor: .res 1
alternate: .res 1
utf8_mode: .res 1
utf_left: .res 1
utf_hi: .res 1
utf_lo: .res 1
utf_first: .res 1
utf_min: .res 1
utf_max: .res 1
utf_wide: .res 1
g0: .res 1
g1: .res 1
gsel: .res 1
charset_target: .res 1
plus_fg: .res 1
plus_bg: .res 1
plus_style: .res 1
plus_inverse: .res 1
plus_bold: .res 1
saved_state: .res 12
alt_saved_state: .res 12
tabs: .res 80
plus_state_end:
receive_fast: .res 1
receive_pos: .res 1
plus_tmp: .res 1
plus_digit: .res 1
plus_count: .res 1
plus_i: .res 1
plus_j: .res 1
plus_private: .res 1
plus_param_n: .res 1
plus_params: .res 16
plus_params_hi: .res 16
plus_final: .res 1
plus_charset: .res 1
.segment "CODE"

.proc plus_init
        jsr plus_defaults
        jsr plus_session_init
        jmp plus_color
.endproc
.proc plus_defaults
        lda #0
        ldx #0
@zero:  sta plus_state_start,x
        inx
        cpx #plus_state_end-plus_state_start
        bne @zero
        lda #7
        sta plus_fg
        sta saved_state+2
        lda #0
        sta lf_mode
        ldx #0
@tabs:  txa
        and #7
        cmp #0
        beq :+
        lda #0
        beq :++
:       lda #1
:       sta tabs,x
        inx
        cpx #80
        bne @tabs
        lda #0
        sta ctrl_seq_flg
        sta ctrl_seq_index
        jmp plus_color
.endproc

; The normal ASCII path checks only parser/encoding state before emitting a cell.
.proc plus_process
        lda ctrl_seq_flg
        jne @state
        lda utf8_mode
        beq @plain
        lda temp_char
        bmi @utf
        lda utf_left
        bne @utf
@plain: lda temp_char
        cmp #32
        bcc @c0
        cmp #127
        beq @ret
        lda gsel
        beq :+
        lda g1
        bne @dec
        jmp plus_put
:       lda g0
        beq @put
@dec:   lda temp_char
        cmp #$60
        bcc @put
        cmp #$7f
        bcs @put
        sec
        sbc #$60
        tax
        lda dec_graphics,x
        sta temp_char
@put:   jmp plus_put
@utf:   jmp plus_utf8
@c0:    cmp #27
        beq @esc
        cmp #7
        beq @bell
        cmp #8
        beq @bs
        cmp #9
        beq @tab
        cmp #10
        beq @lf
        cmp #11
        beq @lf
        cmp #12
        beq @lf
        cmp #13
        beq @cr
        cmp #14
        beq @so
        cmp #15
        beq @si
@ret:   rts
@bell:  jmp BEL_adr
@bs:    lda #0
        sta wrap_pending
        jmp BS_adr
@tab:   lda #0
        sta wrap_pending
        jsr cursor_off
        ldx column
@tn:    cpx term_last_col
        bcs @td
        inx
        lda tabs,x
        beq @tn
@td:    stx column
        jmp plus_position
@lf:    jmp LF_adr
@cr:    lda #0
        sta wrap_pending
        jmp CR_adr
@so:    lda #1
        sta gsel
        rts
@si:    lda #0
        sta gsel
        rts
@esc:   lda #1
        sta ctrl_seq_flg
        rts
@state: jmp plus_state
.endproc

.proc plus_state
        lda temp_char
        cmp #24
        beq @cancel
        cmp #26
        beq @cancel
        lda ctrl_seq_flg
        cmp #7
        jeq u_dcs_process
        cmp #1
        beq @esc
        cmp #2
        beq @csi
        cmp #3
        beq @string
        cmp #4
        beq @st
        cmp #5
        beq @charset
        cmp #6
        beq @encoding
@cancel:lda #0
        sta ctrl_seq_flg
        rts
@string:lda temp_char
        cmp #7
        beq @cancel
        cmp #27
        bne @ret
        lda #4
        sta ctrl_seq_flg
@ret:   rts
@st:    lda temp_char
        cmp #'\'
        beq @cancel
        lda #3
        sta ctrl_seq_flg
        rts
@charset:
        lda #0
        ldx temp_char
        cpx #'0'
        bne :+
        lda #1
:       ldx charset_target
        sta g0,x
        jmp @cancel
@encoding:
        lda temp_char
        cmp #'G'
        beq :+
        cmp #'8'
        beq :+
        lda #0
        sta utf8_mode
        jmp @cancel
:       lda #1
        sta utf8_mode
        jmp @cancel
@esc:   jmp plus_escape
@csi:   lda temp_char
        cmp #27
        beq @restart
        cmp #32
        bcc @ret
        cmp #127
        bcs @cancel
        ldx ctrl_seq_index
        cpx #126
        bcs @overflow
        sta ctrl_seq_buf,x
        inx
        stx ctrl_seq_index
        cmp #64
        bcc @ret
        sta final_byte
        lda #0
        sta ctrl_seq_flg
        jmp plus_csi
@overflow:
        ; Discard an overlong CSI through its final byte, never wrap the buffer.
        cmp #64
        jcs @cancel
        rts
@restart:
        lda #1
        sta ctrl_seq_flg
        rts
.endproc

.proc plus_escape
        lda #0
        sta ctrl_seq_flg
        lda temp_char
        cmp #'['
        beq @csi
        cmp #']'
        beq @str
        cmp #'P'
        jeq u_dcs_start
        cmp #'_'
        beq @str
        cmp #'^'
        beq @str
        cmp #'('
        beq @g0
        cmp #')'
        beq @g1
        cmp #'%'
        beq @enc
        cmp #'7'
        beq @save
        cmp #'8'
        beq @restore
        cmp #'D'
        beq @ind
        cmp #'M'
        beq @ri
        cmp #'E'
        beq @nel
        cmp #'H'
        beq @tab
        cmp #'c'
        beq @reset
        cmp #'Z'
        beq @da
        rts
@csi:   lda #2
        sta ctrl_seq_flg
        lda #0
        sta ctrl_seq_index
        rts
@str:   lda #3
        sta ctrl_seq_flg
        rts
@g0:    lda #0
        beq @g
@g1:    lda #1
@g:     sta charset_target
        lda #5
        sta ctrl_seq_flg
        rts
@enc:   lda #6
        sta ctrl_seq_flg
        rts
@save:  jmp plus_save
@restore:jmp plus_restore
@ind:   jmp plus_index
@ri:    jmp plus_reverse_index
@nel:   jsr CR_adr
        jmp plus_index
@tab:   ldx column
        lda #1
        sta tabs,x
        rts
@reset: jmp plus_reset_terminal
@da:    jmp plus_da
.endproc

.proc plus_put
        jsr cursor_off
        lda wrap_pending
        beq @write
        lda #0
        sta wrap_pending
        sta column
        jsr plus_index
@write: lda insert_mode
        beq :+
        jsr row_insert_char
:       lda temp_char
        jsr to_glyph
        ldy #0
        sta (cursor_address),y
        iny
        lda text_color
        sta (cursor_address),y
        jsr plus_cell_style
        jsr u_hook
        lda column
        cmp term_last_col
        bcs @edge
        inc column
        lda cursor_address
        clc
        adc #2
        sta cursor_address
        bcc @done
        inc cursor_address+1
@done:  jmp cursor_on
@edge:  lda wrap_disabled
        bne @done
        lda atascii_mode
        bne @immediate
        lda #1
        sta wrap_pending
        bne @done
@immediate:
        lda #0
        sta column
        jsr plus_index
        jmp cursor_on
.endproc

.proc plus_position
        lda #0
        sta wrap_pending
        jsr recalc_cursor
        jmp cursor_on
.endproc

.proc plus_index
        jsr cursor_off
        lda #0
        sta wrap_pending
        lda row
        cmp scroll_bot
        beq @scroll
        cmp #23
        bcs @done
        inc row
@done:  jmp plus_position
@scroll:
        lda scroll_top
        bne @region
        lda scroll_bot
        cmp #23
        bne @region
        jsr plus_history_push
        jsr scroll_1d
        lda #0
        ldx #23
        ldy #1
        jsr plus_styles_up
        jmp @done
@region:
        lda scroll_top
        ldx scroll_bot
        ldy #1
        jsr scroll_rgn_up
        jmp @done
.endproc

.proc plus_reverse_index
        jsr cursor_off
        lda row
        cmp scroll_top
        beq @scroll
        cmp #0
        beq @done
        dec row
@done:  jmp plus_position
@scroll:
        lda scroll_top
        ldx scroll_bot
        ldy #1
        jsr scroll_rgn_down
        jmp @done
.endproc

; Parse up to 16 unsigned 16-bit parameters. Saturate instead of wrapping.
.proc plus_parse
        lda #0
        sta plus_private
        sta plus_param_n
        ldx #15
@zero:  sta plus_params,x
        sta plus_params_hi,x
        dex
        bpl @zero
        ldx #0
        lda ctrl_seq_buf
        cmp #'?'
        beq @private
        cmp #'>'
        beq @private
        cmp #'!'
        bne @loop
@private:
        sta plus_private
        inx
@loop:  lda ctrl_seq_buf,x
        cmp #';'
        jeq @next
        cmp #':'
        jeq @next
        cmp #'0'
        jcc @done
        cmp #'9'+1
        jcs @done
        sec
        sbc #'0'
        sta plus_digit
        ldy plus_param_n
        lda plus_params_hi,y
        cmp #25
        bcc @multiply
        bne @sat
        lda plus_params,y
        cmp #153
        bcc @multiply
        bne @sat
        lda plus_digit
        cmp #6
        bcs @sat
@multiply:
        lda plus_params,y
        sta plus_tmp
        asl
        sta plus_params,y
        lda plus_params_hi,y
        rol
        sta plus_j
        lda plus_params,y
        asl
        sta plus_params,y
        lda plus_j
        rol
        sta plus_j
        lda plus_params,y
        clc
        adc plus_tmp
        sta plus_params,y
        lda plus_j
        adc plus_params_hi,y
        sta plus_j
        lda plus_params,y
        asl
        sta plus_params,y
        lda plus_j
        rol
        sta plus_params_hi,y
        lda plus_params,y
        clc
        adc plus_digit
        sta plus_params,y
        bcc @advance
        lda plus_params_hi,y
        adc #0
        sta plus_params_hi,y
        jmp @advance
@sat:   lda #255
        sta plus_params,y
        sta plus_params_hi,y
@advance:
        inx
        cpx ctrl_seq_index
        jcc @loop
@done:  inc plus_param_n
        rts
@next:  inc plus_param_n
        lda plus_param_n
        cmp #16
        bcc :+
        rts
:       inx
        jmp @loop
.endproc

.proc plus_p0
        lda plus_params_hi
        bne @max
        lda plus_params
        bne @ret
        lda #1
@ret:   rts
@max:   lda #255
        rts
.endproc

.proc plus_csi
        jsr plus_parse
        lda final_byte
        sta plus_final
        cmp #'m'
        bne :+
        jmp plus_sgr
:       cmp #'h'
        beq @mode
        cmp #'l'
        beq @mode
        cmp #'n'
        beq @status
        cmp #'c'
        beq @da
        lda plus_private
        bne @ret
        lda final_byte
        ldx #0
@scan:  cmp plus_dispatch,x
        beq @jump
        inx
        cpx #plus_dispatch_end-plus_dispatch
        bcc @scan
@ret:   rts
@jump:  txa
        asl
        tax
        lda plus_vectors+1,x
        pha
        lda plus_vectors,x
        pha
        rts
@mode:  jmp plus_mode
@status:jmp plus_status
@da:    jmp plus_da
.endproc
plus_dispatch: .byte "ABCDEFGHfJKLMPSTX@drsu`g"
plus_dispatch_end:
plus_vectors:
.addr plus_up-1,plus_down-1,plus_right-1,plus_left-1,plus_nextline-1,plus_prevline-1,plus_hpa-1,plus_cup-1,plus_cup-1
.addr plus_ed-1,plus_el-1,plus_il-1,plus_dl-1,plus_dch-1,plus_su-1,plus_sd-1,plus_ech-1,plus_ich-1,plus_vpa-1,plus_margins-1,plus_save-1,plus_restore-1,plus_hpa-1,plus_tabs-1

.proc plus_up
        jsr cursor_off
        jsr plus_p0
        sta plus_count
        lda row
        sec
        sbc plus_count
        bcs :+
        lda #0
:       ldx origin_mode
        beq :+
        cmp scroll_top
        bcs :+
        lda scroll_top
:       sta row
        jmp plus_position
.endproc
.proc plus_down
        jsr cursor_off
        jsr plus_p0
        clc
        adc row
        bcs @max
        cmp #24
        bcc :+
@max:   lda #23
:       ldx origin_mode
        beq :+
        cmp scroll_bot
        bcc :+
        lda scroll_bot
:       sta row
        jmp plus_position
.endproc
.proc plus_right
        jsr cursor_off
        jsr plus_p0
        clc
        adc column
        bcs @max
        cmp term_last_col
        bcc :+
@max:   lda term_last_col
:       sta column
        jmp plus_position
.endproc
.proc plus_left
        jsr cursor_off
        jsr plus_p0
        sta plus_count
        lda column
        sec
        sbc plus_count
        bcs :+
        lda #0
:       sta column
        jmp plus_position
.endproc
plus_nextline: jsr plus_down
        jmp CR_adr
plus_prevline: jsr plus_up
        jmp CR_adr
.proc plus_hpa
        jsr cursor_off
        jsr plus_p0
        sec
        sbc #1
        cmp term_last_col
        bcc :+
        lda term_last_col
:       sta column
        jmp plus_position
.endproc
.proc plus_vpa
        jsr cursor_off
        jsr plus_p0
        sec
        sbc #1
        ldx origin_mode
        beq :+
        clc
        adc scroll_top
        bcs @max
        cmp scroll_bot
        bcc @store
@max:   lda scroll_bot
        bne @store
:       cmp #24
        bcc @store
        lda #23
@store: sta row
        jmp plus_position
.endproc
.proc plus_cup
        jsr plus_vpa
        lda plus_params+1
        sta plus_params
        lda plus_params_hi+1
        sta plus_params_hi
        jmp plus_hpa
.endproc
.proc plus_margins
        jsr plus_p0
        sec
        sbc #1
        cmp #23
        bcs @ret
        sta plus_tmp
        lda plus_params_hi+1
        bne @ret
        lda plus_params+1
        bne :+
        lda #24
:       cmp #25
        bcs @ret
        sec
        sbc #1
        cmp plus_tmp
        beq @ret
        bcc @ret
        sta scroll_bot
        lda plus_tmp
        sta scroll_top
        jsr cursor_off
        lda #0
        sta row
        sta column
        lda origin_mode
        beq :+
        lda scroll_top
        sta row
:       jmp plus_position
@ret:   rts
.endproc

; Character editing is bounded to the current row.
.proc plus_edit_count
        jsr plus_p0
        sta plus_count
        lda term_last_col
        sec
        sbc column
        clc
        adc #1
        cmp plus_count
        bcs :+
        sta plus_count
:       rts
.endproc
.proc plus_ich
        jsr plus_edit_count
        jsr cursor_off
        jsr plus_insert_chars
        jmp plus_position
.endproc
.proc plus_dch
        jsr plus_edit_count
        jsr cursor_off
        jsr plus_delete_chars
        jmp plus_position
.endproc
.proc plus_ech
        jsr plus_edit_count
        jsr cursor_off
        jsr plus_erase_chars
        jmp plus_position
.endproc
.proc plus_el
        lda plus_params
        cmp #2
        bcs @all
        beq @all
        cmp #1
        beq @left
        lda #255
        sta plus_params
        jmp plus_ech
@left:  lda column
        clc
        adc #1
        sta plus_params
        jmp @fromstart
@all:   lda term_last_col
        clc
        adc #1
        sta plus_params
@fromstart:
        lda column
        pha
        jsr cursor_off
        lda #0
        sta column
        jsr recalc_cursor
        jsr plus_ech
        jsr cursor_off
        pla
        sta column
        jmp plus_position
.endproc
.proc plus_ed
        lda plus_params
        cmp #3
        beq @history
        cmp #2
        beq @all
        cmp #1
        beq @above
        jsr plus_el
        lda #23
        sec
        sbc row
        tax
        lda row
        clc
        adc #1
        jsr blank_rows
        jmp plus_dirty_all
@above: jsr plus_el
        ldx row
        lda #0
        jsr blank_rows
        jmp plus_dirty_all
@all:   jsr cursor_off
        jsr scroll_page
        jsr plus_dirty_all
        jmp plus_position
@history:
        jmp plus_history_clear
.endproc
.proc plus_il
        lda row
        cmp scroll_top
        bcc @ret
        cmp scroll_bot
        bcc :+
        bne @ret
:       jsr cursor_off
        jsr plus_p0
        tay
        lda row
        ldx scroll_bot
        jsr scroll_rgn_down
        jsr plus_dirty_all
        jmp plus_position
@ret:   rts
.endproc
.proc plus_dl
        lda row
        cmp scroll_top
        bcc @ret
        cmp scroll_bot
        bcc :+
        bne @ret
:       jsr cursor_off
        jsr plus_p0
        tay
        lda row
        ldx scroll_bot
        jsr scroll_rgn_up
        jsr plus_dirty_all
        jmp plus_position
@ret:   rts
.endproc
.proc plus_su
        jsr cursor_off
        jsr plus_p0
        tay
        lda scroll_top
        ldx scroll_bot
        jsr scroll_rgn_up
        jsr plus_dirty_all
        jmp plus_position
.endproc
.proc plus_sd
        jsr cursor_off
        jsr plus_p0
        tay
        lda scroll_top
        ldx scroll_bot
        jsr scroll_rgn_down
        jsr plus_dirty_all
        jmp plus_position
.endproc
.proc plus_tabs
        lda plus_params
        beq @one
        cmp #3
        bne @ret
        ldx #79
        lda #0
@all:   sta tabs,x
        dex
        bpl @all
@ret:   rts
@one:   ldx column
        sta tabs,x
        rts
.endproc

.proc plus_save
        lda row
        sta saved_state
        lda column
        sta saved_state+1
        lda plus_fg
        sta saved_state+2
        lda plus_bg
        sta saved_state+3
        lda plus_style
        sta saved_state+4
        lda plus_inverse
        sta saved_state+5
        lda plus_bold
        sta saved_state+6
        lda wrap_pending
        sta saved_state+7
        lda origin_mode
        sta saved_state+8
        lda g0
        sta saved_state+9
        lda g1
        sta saved_state+10
        lda gsel
        sta saved_state+11
        rts
.endproc
.proc plus_restore
        jsr cursor_off
        lda saved_state
        sta row
        lda saved_state+1
        sta column
        lda saved_state+2
        sta plus_fg
        lda saved_state+3
        sta plus_bg
        lda saved_state+4
        sta plus_style
        lda saved_state+5
        sta plus_inverse
        lda saved_state+6
        sta plus_bold
        lda saved_state+8
        sta origin_mode
        lda saved_state+9
        sta g0
        lda saved_state+10
        sta g1
        lda saved_state+11
        sta gsel
        jsr plus_color
        jsr plus_position
        lda saved_state+7
        sta wrap_pending
        rts
.endproc

.proc plus_mode
        lda #0
        ldx final_byte
        cpx #'h'
        bne :+
        lda #1
:       sta plus_count
        ldx #0
@loop:  stx plus_i
        lda plus_private
        cmp #'?'
        beq @private
        lda plus_params,x
        cmp #4
        bne :+
        lda plus_count
        sta insert_mode
:       lda plus_params,x
        cmp #20
        jne @next
        lda plus_count
        sta lf_mode
        jmp @next
@private:
        lda plus_params_hi,x
        beq @simple
        cmp #4
        beq @alt
        jmp @next
@simple:lda plus_params,x
        cmp #1
        beq @cursor
        cmp #6
        beq @origin
        cmp #7
        beq @wrap
        cmp #25
        beq @visible
        cmp #47
        beq @alternate
        jmp @next
@alt:   lda plus_params,x
        cmp #25
        beq @alternate
        cmp #23
        beq @alternate
        cmp #24
        bne @next
        lda plus_count
        beq :+
        jsr plus_save
        jmp @next
:       jsr plus_restore
        jmp @next
@alternate:
        jsr plus_alternate
        jmp @next
@cursor:lda plus_count
        sta app_cursor
        jmp @next
@origin:lda plus_count
        sta origin_mode
        jsr cursor_off
        lda #0
        sta column
        sta row
        lda origin_mode
        beq :+
        lda scroll_top
        sta row
:       jsr plus_position
        jmp @next
@wrap:  lda plus_count
        eor #1
        sta wrap_disabled
        lda #0
        sta wrap_pending
        jmp @next
@visible:
        jsr cursor_off
        lda plus_count
        eor #1
        sta cursor_hidden
        jsr cursor_on
@next:  ldx plus_i
        inx
        cpx plus_param_n
        jcc @loop
        rts
.endproc

.proc plus_sgr
        lda plus_private
        beq :+
        rts
:       ldx #0
@loop:  stx plus_i
        lda plus_params_hi,x
        bne @next
        lda plus_params,x
        jeq @reset
        cmp #1
        jeq @bold
        cmp #22
        jeq @unbold
        cmp #4
        jeq @underline
        cmp #24
        jeq @nounderline
        cmp #5
        jeq @blink
        cmp #25
        jeq @noblink
        cmp #7
        jeq @reverse
        cmp #27
        jeq @noreverse
        cmp #39
        jeq @fgdefault
        cmp #49
        jeq @bgdefault
        cmp #30
        bcc @next
        cmp #38
        jcc @fg
        cmp #40
        bcc @next
        cmp #48
        jcc @bg
        cmp #90
        bcc @next
        cmp #98
        jcc @brightfg
        cmp #100
        bcc @next
        cmp #108
        jcc @brightbg
@next:  ldx plus_i
        inx
        cpx plus_param_n
        jcc @loop
        jmp plus_color
@reset: lda #7
        sta plus_fg
        lda #0
        sta plus_bg
        sta plus_style
        sta plus_inverse
        sta plus_bold
        jmp @next
@bold:  lda #8
        sta plus_bold
        jmp @next
@unbold:lda #0
        sta plus_bold
        jmp @next
@underline:lda plus_style
        ora #1
        sta plus_style
        jmp @next
@nounderline:lda plus_style
        and #$fe
        sta plus_style
        jmp @next
@blink: lda plus_style
        ora #2
        sta plus_style
        jmp @next
@noblink:lda plus_style
        and #$fd
        sta plus_style
        jmp @next
@reverse:lda #1
        sta plus_inverse
        jmp @next
@noreverse:lda #0
        sta plus_inverse
        jmp @next
@fgdefault:lda #7
        sta plus_fg
        jmp @next
@bgdefault:lda #0
        sta plus_bg
        jmp @next
@fg:    sec
        sbc #30
        sta plus_fg
        jmp @next
@bg:    sec
        sbc #40
        sta plus_bg
        jmp @next
@brightfg:sec
        sbc #82
        sta plus_fg
        jmp @next
@brightbg:sec
        sbc #92
        sta plus_bg
        jmp @next
.endproc

.proc plus_color
        lda plus_fg
        ora plus_bold
        sta draw_fg
        lda plus_bg
        sta draw_bg
        lda plus_inverse
        beq :+
        lda draw_fg
        ldx draw_bg
        sta draw_bg
        stx draw_fg
:       lda draw_bg
        and #7
        asl
        asl
        asl
        asl
        ora draw_fg
        ora #$80
        sta text_color
        rts
.endproc

.proc plus_queue
        php
        sei
        ldx sendbufend
        inx
        cpx sendbufstart
        beq @full
        sta send_buffer,x
        stx sendbufend
@full:  plp
        rts
.endproc
.proc plus_reply
        sta scr_src
        stx scr_src+1
        ldy #0
@loop:  lda (scr_src),y
        beq @ret
        jsr plus_queue
        iny
        bne @loop
@ret:   rts
.endproc
plus_da:
        lda #<reply_da
        ldx #>reply_da
        jmp plus_reply
reply_da: .byte 27,"[?1;0c",0
reply_ok: .byte 27,"[0n",0
.proc plus_decimal
        ldx #0
@tens:  cmp #10
        bcc @last
        sec
        sbc #10
        inx
        bne @tens
@last:  pha
        cpx #0
        beq :+
        txa
        ora #'0'
        jsr plus_queue
:       pla
        ora #'0'
        jmp plus_queue
.endproc
.proc plus_status
        lda plus_params
        cmp #5
        beq @ok
        cmp #6
        bne @ret
        lda #27
        jsr plus_queue
        lda #'['
        jsr plus_queue
        lda plus_private
        cmp #'?'
        bne :+
        jsr plus_queue
:       lda row
        ldx origin_mode
        beq :+
        sec
        sbc scroll_top
:       clc
        adc #1
        jsr plus_decimal
        lda #';'
        jsr plus_queue
        lda column
        clc
        adc #1
        jsr plus_decimal
        lda #'R'
        jmp plus_queue
@ok:    lda #<reply_ok
        ldx #>reply_ok
        jmp plus_reply
@ret:   rts
.endproc

dec_graphics: .byte 4,177,9,12,13,10,248,241,10,11,217,191,218,192,197,196,196,196,196,196,195,180,193,194,179,243,242,227,216,156,250

        .include "plus_utf8.inc"

.proc plus_reset_terminal
        jsr cursor_off
        lda batch_mode
        pha
        jsr plus_defaults
        jsr plus_video_init
        jsr plus_history_clear
        lda #79
        sta term_last_col
        jsr reset_margins
        pla
        sta batch_mode
        jmp FF_adr
.endproc

.proc plus_prepare_fast
        lda #0
        sta receive_fast
        lda draw_bg
        and #8
        ora plus_style
        ora tn_state
        ora tn_cr
        ora ctrl_seq_flg
        ora atascii_mode
        ora utf_left
        ora g0
        ora g1
        ora insert_mode
        ora wrap_pending
        ora wrap_disabled
        ora bitmap_on
        ora capture_on
        bne @out
        lda term_last_col
        cmp #79
        bne @out
        inc receive_fast
@out:   rts
.endproc
