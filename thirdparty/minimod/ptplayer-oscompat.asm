;
; OS compatible CIA timer interrupt setup for ptplayer.
; Written by Harry Sintonen <sintonen@iki.fi>.
; Public Domain.
;

_LVOOpenResource EQU	-498
_LVOAddICRVector EQU	-6
_LVORemICRVector EQU	-12
_LVOAbleICR	EQU	-18

CIAICRB_TA	EQU	0
CIAICRB_TB	EQU	1

	even

;---------------------------------------------------------------------------
	xdef	_mt_install_sys_cia
_mt_install_sys_cia:
; Install a CIA-B interrupt for calling mt_music.
; OUT: d0.l status, 1 for success, 0 for error

	move.l	a6,-(sp)

	ifnd	SDATA
	move.l	a4,-(sp)
	lea	mt_data(pc),a4
	endc

	; attempt to allocate CIA-B timers A and B
	move.l	4.w,a6
	lea	mt_ciab_name(pc),a1
	jsr	_LVOOpenResource(a6)
	tst.l	d0
	beq	.nociares

	move.l	d0,a6

	clr.b	mt_Enable(a4)

	lea	TB_toggle(pc),a0
	clr.b	(a0)

	ifeq	VBLANK_MUSIC
	moveq	#CIAICRB_TA,d0
	lea	mt_extern_timer_a_is(pc),a1
	jsr	_LVOAddICRVector(a6)
	tst.l	d0
	bne	.no_timer_a
	endc	; !VBLANK_MUSIC
	moveq	#CIAICRB_TB,d0
	lea	mt_extern_timer_b_is(pc),a1
	jsr	_LVOAddICRVector(a6)
	tst.l	d0
	bne	.no_timer_b

	; TimerA and TimerB interrupt disable
	ifne	VBLANK_MUSIC
	moveq	#$2,d0			; TimerB only for VBLANK_MUSIC
	else
	moveq	#$3,d0
	endc
	jsr	_LVOAbleICR(a6)

	lea	CIAB,a0

	; Stop Timer B
	moveq	#0,d0
	move.b	d0,CIACRB(a0)

	ifeq	VBLANK_MUSIC
	; Stop Timer A
	move.b	d0,CIACRA(a0)

	; determine if 02 clock for timers is based on PAL or NTSC
	move.l	4.w,a1
	cmp.b	#50,531(a1)		; PowerSupplyFrequency
	beq	.1
	move.l	#1789773,d0             ; NTSC
	bra	.2
.1:	move.l	#1773447,d0             ; PAL
.2:	move.l	d0,mt_timerval(a4)

	; load TimerA in continuous mode for the default tempo of 125
	divu	#125,d0
	move.b	d0,CIATALO(a0)
	lsr.w	#8,d0
	move.b	d0,CIATAHI(a0)
	move.b	#$11,CIACRA(a0)		; load timer, start continuous
	endc	; !VBLANK_MUSIC

	; load TimerB with DMADELAY ticks for setting DMA and repeat
	move.b	#DMADELAY&255,CIATBLO(a0)
	move.b	#DMADELAY>>8,CIATBHI(a0)

	; TimerA and TimerB interrupt enable
	ifne	VBLANK_MUSIC
	move.w	#$82,d0			; TimerB only for VBLANK_MUSIC
	else
	move.w	#$83,d0
	endc
	jsr	_LVOAbleICR(a6)

	pea	.popout(pc)
	ifnd	SDATA
	move.l	a4,-(sp)
	endc
	lea	CUSTOM,a6
	bra	mt_reset
.popout:
	moveq	#1,d0			; Indicate success
.out:
	ifnd	SDATA
	move.l	(sp)+,a4
	endc
	move.l	(sp)+,a6
	rts

.no_timer_b:
	ifeq	VBLANK_MUSIC
	moveq	#CIAICRB_TA,d0
	lea	mt_extern_timer_a_is(pc),a1
	jsr	_LVORemICRVector(a6)
.no_timer_a:
	endc	; !VBLANK_MUSIC
.nociares:
	moveq	#0,d0			; Indicate failure
	bra	.out


;---------------------------------------------------------------------------
	xdef	_mt_remove_sys_cia
_mt_remove_sys_cia:
; Remove a CIA-B interrupt for calling mt_music.

	move.l	a6,-(sp)

	move.l	4.w,a6
	lea	mt_ciab_name(pc),a1
	jsr	_LVOOpenResource(a6)
	tst.l	d0
	beq	.nociares

	move.l	d0,a6

	; TimerA and TimerB interrupt disable
	ifne	VBLANK_MUSIC
	moveq	#$02,d0			; TimerB only for VBLANK_MUSIC
	else
	moveq	#$03,d0
	endc
	jsr	_LVOAbleICR(a6)

	moveq	#CIAICRB_TB,d0
	lea	mt_extern_timer_b_is(pc),a1
	jsr	_LVORemICRVector(a6)
	ifeq	VBLANK_MUSIC
	moveq	#CIAICRB_TA,d0
	lea	mt_extern_timer_a_is(pc),a1
	jsr	_LVORemICRVector(a6)
	endc	; !VBLANK_MUSIC

.nociares:
	move.l	(sp)+,a6
	rts

;---------------------------------------------------------------------------

	ifeq	VBLANK_MUSIC
mt_extern_timer_a_is:
	dc.l	0,0
	dc.b	2	; NT_INTERRUPT
	dc.b	0	; priority
	dc.l	mt_timer_int_name
	dc.l	0
	dc.l	mt_cia_timer_a_code
	endc	; !VBLANK_MUSIC

mt_extern_timer_b_is:
	dc.l	0,0
	dc.b	2	; NT_INTERRUPT
	dc.b	0	; priority
	dc.l	mt_timer_int_name
	dc.l	0
	dc.l	mt_cia_timer_b_code

mt_ciab_name:
	dc.b	"ciab.resource",0
mt_timer_int_name:
	dc.b	"ptplayer timer",0
	even

;---------------------------------------------------------------------------

	ifeq	VBLANK_MUSIC
mt_cia_timer_a_code:
	movem.l	d2-d7/a2-a6,-(sp)
	lea	CUSTOM,a6
	ifd	SDATA
	lea	_LinkerDB,a4
	else
	lea	mt_data(pc),a4
	endc

	; do music when enabled
	tst.b	mt_Enable(a4)
	ifeq	MINIMAL
	beq	.2
	else
	beq	.1
	endc

	bsr	mt_music		; music with sfx inserted
.1:	movem.l (sp)+,d2-d7/a2-a6
	rts

	ifeq	MINIMAL
.2:	bsr	mt_sfxonly		; no music, only sfx
	movem.l	(sp)+,d2-d7/a2-a6
	rts
	endc
	endc	; !VBLANK_MUSIC

;---------------------------------------------------------------------------

mt_cia_timer_b_code:
; Handle one-shot TimerB interrupt.
; TB_toggle-technique suggested by Ross/EAB.

	lea	TB_toggle(pc),a0
	not.b	(a0)
	lea	CUSTOM+INTREQ,a0
	beq	mt_TimerBsetrep

	; restart timer for repeat, enable audio DMA after DMADELAY ticks
	move.b	#$19,CIAB+CIACRB
	move.w	mt_dmaon(pc),DMACON-INTREQ(a0)
	rts

mt_dmaon:
	dc.w	$8000
TB_toggle:
	dc.b    0
	even

mt_TimerBsetrep:
; Oneshot TimerB interrupt to set repeat samples after another DMADELAY ticks.
; a0 = INTREQ

	move.l	a4,-(sp)

	ifd	SDATA
	lea	_LinkerDB,a4
	else
	lea	mt_data(pc),a4
	endc

	; clear possible audio interrupt flags
	moveq	#0,d0
	or.b	mt_dmaon+1(pc),d0
	lsl.w	#7,d0
	move.w	d0,(a0)

	; set repeat sample pointers and lengths
	ifne	MINIMAL
	move.l	mt_chan1+n_loopstart(a4),AUD0LC-INTREQ(a0)
	move.w	mt_chan1+n_replen(a4),AUD0LEN-INTREQ(a0)
	move.l	mt_chan2+n_loopstart(a4),AUD1LC-INTREQ(a0)
	move.w	mt_chan2+n_replen(a4),AUD1LEN-INTREQ(a0)
	move.l	mt_chan3+n_loopstart(a4),AUD2LC-INTREQ(a0)
	move.w	mt_chan3+n_replen(a4),AUD2LEN-INTREQ(a0)
	move.l	mt_chan4+n_loopstart(a4),AUD3LC-INTREQ(a0)
	move.w	mt_chan4+n_replen(a4),AUD3LEN-INTREQ(a0)

	else	; !MINIMAL
	tst.b	mt_chan1+n_enable(a4)
	beq	.1
	move.l	mt_chan1+n_loopstart(a4),AUD0LC-INTREQ(a0)
	move.w	mt_chan1+n_replen(a4),AUD0LEN-INTREQ(a0)
.1:	tst.b	mt_chan2+n_enable(a4)
	beq	.2
	move.l	mt_chan2+n_loopstart(a4),AUD1LC-INTREQ(a0)
	move.w	mt_chan2+n_replen(a4),AUD1LEN-INTREQ(a0)
.2:	tst.b	mt_chan3+n_enable(a4)
	beq	.3
	move.l	mt_chan3+n_loopstart(a4),AUD2LC-INTREQ(a0)
	move.w	mt_chan3+n_replen(a4),AUD2LEN-INTREQ(a0)
.3:	tst.b	mt_chan4+n_enable(a4)
	beq	.4
	move.l	mt_chan4+n_loopstart(a4),AUD3LC-INTREQ(a0)
	move.w	mt_chan4+n_replen(a4),AUD3LEN-INTREQ(a0)
.4:
	endc

	move.l	(sp)+,a4
	rts

;---------------------------------------------------------------------------

