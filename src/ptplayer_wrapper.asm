        SECTION CODE,CODE

        xdef    _maja_pt_install
        xdef    _maja_pt_start
        xdef    _maja_pt_stop

_maja_pt_install:
        movem.l d2-d7/a2-a6,-(sp)
        lea     $dff000,a6
        jsr     _mt_install_sys_cia
        movem.l (sp)+,d2-d7/a2-a6
        rts

_maja_pt_start:
        movem.l d2-d7/a2-a6,-(sp)
        move.l  48(sp),a0
        move.l  52(sp),a1
        moveq   #0,d0
        lea     $dff000,a6
        jsr     _mt_init
        st      _mt_Enable
        movem.l (sp)+,d2-d7/a2-a6
        rts

_maja_pt_stop:
        movem.l d2-d7/a2-a6,-(sp)
        lea     $dff000,a6
        jsr     _mt_end
        jsr     _mt_remove_sys_cia
        movem.l (sp)+,d2-d7/a2-a6
        rts

MINIMAL         EQU     1
OSCOMPAT        EQU     1
        include "../thirdparty/minimod/ptplayer.asm"
