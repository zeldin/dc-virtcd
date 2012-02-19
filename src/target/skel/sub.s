
	.globl	start
	
	.text

	.align 2

start:	
	rts
	nop
	bra enable
	nop
	bra disable
	nop
	bra reset
	nop


enable:
	mova	handler,r0
	mov.l	isysc_vector,r1
	mov.l	@r1,r2
	cmp/eq	r0,r1
	bt	.done_already
	mov.l	r0,@r1
	mov.l	r2,@(real_vector-handler,r0)
.done_already:
	mov.l	bss_start_addr,r0
	mov.l	bss_end_addr,r2
	mov	#3,r1
	add	r1,r0
	add	r1,r2
	not	r1,r1
	and	r1,r0
	and	r1,r2
	sub	r0,r2
	shlr	r2
	shlr	r2
	mov	#0,r1
.loop:	dt	r2
	mov.l	r1,@r0
	bf/s	.loop
	add	#4,r0
	mov.l	mainaddr,r0
	jmp	@r0
	nop

disable:
	rts
	nop

reset:
	rts
	nop

	.align 4

mainaddr:
	.long _init
	

	.align 4

handler:
	tst r6,r6
	bt .new_syscall
	mov.l real_vector,r0
	jsr @r0
	sts.l pr,@-r15
	mov.l isysc_vector,r4
	lds.l @r15+,pr
	mov.l iinstall_addr,r1
	rts
	mov.l r1,@r4

.new_syscall:
	mov #16,r0
	cmp/hi	r0,r7
	bt .badsysc
	shll2 r7
	mova jtable,r0
	mov.l @(r0,r7),r0
	shlr r7
	jmp @r0
	shlr r7
.badsysc:	
	rts
	mov #-1,r0
	

	.align 4

real_vector:
	.long	0
isysc_vector:
	.long	0x8c0000bc
iinstall_addr:
	.long	handler
bss_start_addr:
	.long	__bss_start
bss_end_addr:
	.long	_end

jtable:
	.long	_gdrom_send_command	! 0
	.long	_gdrom_check_command
	.long	_gdrom_mainloop
	.long	_gdrom_init
	.long	_gdrom_check_drive	! 4
	.long	_unimpl_syscall
	.long	_unimpl_syscall
	.long	_unimpl_syscall
	.long	_unimpl_syscall	! 8
	.long	_gdrom_reset
	.long	_gdrom_sector_mode
	.long	_unimpl_syscall
	.long	_unimpl_syscall	! 12
	.long	_unimpl_syscall
	.long	_unimpl_syscall
	.long	_unimpl_syscall

	
	

