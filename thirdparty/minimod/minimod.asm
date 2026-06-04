; A minimal ProTracker modplayer
; Written by Harry Sintonen <sintonen@iki.fi>
;
; Uses ptplayer from Frank Wille. You can grab the original source code
; from: https://aminet.net/package/mus/play/ptplayer
;
; Known problems
; --------------
;
; - The commandline argument must only contain the name of the
;   file to load, no quotes, extra spaces or some such.
;
; - There is no checking whether the file provided is a mod or not.
;   It is just sent to the player routine as-is.
;
; - The access to the audio hardware isn't arbitrated. If something
;   else is using the audio things will get screwy.
;
; To Compile:
; 1) phxass minimod.asm exe z
;
;
; Version history
;
; 1.0   - Initial version.
; 1.1   - Added loading module header and pattern data to any memory.
;         Allocate the ciab.resource timer A and B to avoid conflicts.
;         Fixed a bug in the ptplayer level 6 interrupt routine.
; 1.2   - Implemented OS friendly CIA interrupt setup resulting in far
;         improved stability due to ICR no longer being poked directly.
;

_LVOAllocMem	EQU	-198
_LVOFreeMem	EQU	-210
_LVOOpenLibrary	EQU	-552
_LVOCloseLibrary EQU	-414
_LVOWait	EQU	-318

MEMF_ANY	EQU	0
MEMF_CHIP	EQU	2

_LVOOpen	EQU	-30
_LVOClose	EQU	-36
_LVORead	EQU	-42
_LVOWrite	EQU	-48
_LVOOutput	EQU	-60
_LVOLock	EQU	-84
_LVOUnLock	EQU	-90
_LVOExamine	EQU	-102

ACCESS_READ	EQU	-2
MODE_OLDFILE	EQU	1005
fib_Size	EQU	124
fib_SIZEOF	EQU	260

SIGBREAKF_CTRL_C EQU	$1000

; ProTracker mod header constants
PTH_ORDERLIST	EQU	952
PTH_SIZEOF	EQU	1084
; one pattern is 64 rows of 4 channels with 4 bytes per channel
PATTERNSIZE	EQU	64*4*4

; localdata pointed by a4
l_size		EQU	0
l_ptheaderptr	EQU	4	; |
l_sampleptr	EQU	8	; | order of these 3 matters.
l_samplesize	EQU	12	; |
l_fib		EQU	16
l_ptheader	EQU	16
l_SIZEOF	EQU	16+PTH_SIZEOF


main:
	lea	_localdata,a4

	moveq	#20,d7

	; remove commandline linefeed
	clr.b	-1(a0,d0.l)
	; save commandline
	move.l	a0,a2

	move.l	(4).w,a6

	lea	(dosname,pc),a1
	moveq	#0,d0
	jsr	(_LVOOpenLibrary,a6)
	tst.l	d0
	beq	.nodos
	move.l	a6,a5
	move.l	d0,a6

	; lock the file, examine, unlock
	move.l	a2,d1
	moveq	#ACCESS_READ,d2
	jsr	(_LVOLock,a6)
	move.l	d0,d3
	beq	.nolock
	move.l	d3,d1
	lea	(l_fib,a4),a0
	move.l	a0,d2
	jsr	(_LVOExamine,a6)
	move.l	d0,d1
	exg	d3,d1
	jsr	(_LVOUnLock,a6)
	tst.l	d3
	beq	.noexamine

	move.l	a2,d1
	move.l	#MODE_OLDFILE,d2
	jsr	(_LVOOpen,a6)
	move.l	d0,d5
	beq	.nofile

	; copy the fib_Size since we will reuse the buffer
	move.l	(l_fib+fib_Size,a4),(l_size,a4)

	move.l	d5,d1
	lea	(l_ptheader,a4),a0
	move.l	a0,d2
	move.l	#PTH_SIZEOF,d3
	jsr	(_LVORead,a6)
	cmp.l	d0,d3
	bne	.headerfail

	; find the highest pattern number
	lea	(l_ptheader+PTH_ORDERLIST,a4),a0
	moveq	#0,d3
	moveq	#128-1,d0
.scan:	cmp.b	(a0)+,d3
	bhi.b	.next
	move.b	(-1,a0),d3
.next:	dbf	d0,.scan
	; add 1 to include the size of the last pattern
	addq.l	#1,d3

	; calculate the pattern data size
	;mulu.w	#PATTERNSIZE,d3
	moveq	#10,d0		; 1<<10 == PATTERNSIZE
	lsl.l	d0,d3
	; calculate the total size of the header + pattern data
	move.l	#PTH_SIZEOF,d2
	add.l	d3,d2

	; calculate the sample buffer size
	move.l	(l_size,a4),d0
	sub.l	d2,d0
	bmi	.headerfail	; file too small?
	move.l	d0,(l_samplesize,a4)

	; allocate sample buffer
	; d0 = chipmem alloc size
	moveq	#MEMF_CHIP,d1
	exg	a5,a6
	jsr	(_LVOAllocMem,a6)
	move.l	d0,(l_sampleptr,a4)
	; allocate header + pattern data buffer
	move.l	d2,d0
	moveq	#MEMF_ANY,d1
	jsr	(_LVOAllocMem,a6)
	move.l	d0,(l_ptheaderptr,a4)
	exg	a5,a6
	beq	.nomem
	tst.l	(l_sampleptr,a4)
	beq	.nomem

	; copy the header from the buffer
	lea	(l_ptheader,a4),a0
	move.l	(l_ptheaderptr,a4),a1
	move.w	#(PTH_SIZEOF>>2)-1,d0
.hcopy:	move.l	(a0)+,(a1)+
	dbf	d0,.hcopy

	; read the pattern data
	move.l	d5,d1
	move.l	a1,d2
	; d3 = pattern data size from before
	jsr	(_LVORead,a6)
	cmp.l	d0,d3
	bne	.readfail

	; read the sample data
	move.l	d5,d1
	movem.l	(l_sampleptr,a4),d2/d3	; load l_sampleptr / l_samplesize to d2/d3
	jsr	(_LVORead,a6)
	cmp.l	d0,d3
	bne.b	.readfail

	; everything was loaded correctly, show a message
	jsr	(_LVOOutput,a6)
	move.l	d0,d1
	lea	(msg,pc),a0
	move.l	a0,d2
	moveq	#msgend-msg,d3
	jsr	(_LVOWrite,a6)

	move.l	a6,-(sp)
	lea	$dff000,a6

	jsr	_mt_install_sys_cia
	tst.l	d0
	beq.b	.initerr

	; loads l_ptheaderptr and l_sampleptr to a0/a1
	movem.l	(l_ptheaderptr,a4),a0/a1
	moveq	#0,d0	; songpos
	jsr	_mt_init

	; start playback
	st	_mt_Enable

	; Wait until CTRL-C
	move.l	#SIGBREAKF_CTRL_C,d0
	exg	a5,a6
	jsr	(_LVOWait,a6)
	exg	a5,a6

	jsr	_mt_end

	jsr	_mt_remove_sys_cia

	; all ok
	moveq	#0,d7

.initerr:
	move.l	(sp)+,a6

.readfail:
.nomem:
	exg	a5,a6
	move.l	(l_samplesize,a4),d2
	move.l	(l_ptheaderptr,a4),d0
	beq.b	.nm_head
	move.l	d0,a1
	move.l	(l_size,a4),d0
	sub.l	d2,d0
	jsr	(_LVOFreeMem,a6)
.nm_head:
	move.l	(l_sampleptr,a4),d0
	beq.b	.nm_samples
	move.l	d0,a1
	move.l	d2,d0
	jsr	(_LVOFreeMem,a6)
.nm_samples:
	exg	a5,a6
.headerfail:
	move.l	d5,d1
	jsr	(_LVOClose,a6)
.nofile:
.noexamine:
.nolock:
	move.l	a6,a1
	move.l	a5,a6
	jsr	(_LVOCloseLibrary,a6)
.nodos:
	move.l	d7,d0
.timercode:
	rts

dosname:
	dc.b	"dos.library",0
msg:
	dc.b	"minimod 1.2 by Harry Sintonen. Public Domain.",10
	dc.b	$bb,$bb,$bb
	dc.b	" Press CTRL-C to exit "
	dc.b	$ab,$ab,$ab,10
msgend:
	CNOP	0,2

;---------------------------------------------------------------------------

	SECTION BSS,BSS

_localdata:
	ds.b	l_SIZEOF

;---------------------------------------------------------------------------

	SECTION CODE,CODE

MINIMAL		EQU	1
OSCOMPAT	EQU	1

	include	"ptplayer.asm"
