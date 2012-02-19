
	.globl	_launch

	.text
	.align 2
		
_launch:
	mov	r4,r14

	mov.l	cc2ptr,r2
	mov.l	ncmask,r3
	or	r3,r2
	jsr	@r2
	nop

	stc	sr,r0
	mov.w	imask1,r1
	and	r1,r0
	or	#-16,r0
	ldc	r0,sr

	mov.l	newsp,r0
	mov	r0,r15

	mov.l	ms_strt,r4
	mov.w	ms_len,r6
	bsr	memset
	mov	#0,r5

	mov.l	newsr,r0
	ldc	r0,sr

	sub	r0,r0
	mov	r0,r1
	mov	r0,r2
	mov	r0,r3
	mov	r0,r4
	mov	r0,r5
	mov	r0,r6
	mov	r0,r7
	mov	r0,r8
	mov	r0,r9
	mov	r0,r10
	mov	r0,r11
	mov	r0,r12
	mov	r0,r13

	mov.l	newsp2,r0
	mov	r0,r15
	mov.l	newvbr,r0
	ldc	r0,vbr
	mov.l	newfpscr,r0
	lds	r0,fpscr

	mov	r1,r0
	jsr	@r14
	mov	r1,r14

foo:	
	mov.l	startaddr,r0
	jmp	@r0
	nop


	.align	4

ccache2:
	mov.l	dcptr2,r0
	mov.l	@r0,r1
	mov.l	dcmask,r2
	and	r2,r1
	mov.w	dcval,r2
	or	r2,r1
	mov.l	r1,@r0
	rts
	nop

memset:
	mov	#0,r7
	mov	r7,r3
	cmp/hs	r6,r3
	bt/s	.mmbye
	mov	r4,r0
.mmlp:	add	#1,r7
	mov.b	r5,@r0
	cmp/hs	r6,r7
	bf/s	.mmlp
	add	#1,r0
.mmbye:	rts
	mov	r4,r0
	
	.align	4

cc2ptr:
	.long	ccache2
ncmask:
	.long	0xa0000000
newsp:
	.long	0xac00f400
ms_strt:
	.long	0xac00fc00
newsr:
	.long	0x700000f0
newsp2:
	.long	0x8c00f400
newvbr:
	.long	0x8c00f400
startaddr:
	.long	start
newfpscr:
	.long	0x40001
dcptr2:
	.long	0xff00001c
dcmask:
	.long	0x89af
dcval:
	.word	0x800
ms_len:
	.word	0x400
imask1:
	.word	0xff0f


