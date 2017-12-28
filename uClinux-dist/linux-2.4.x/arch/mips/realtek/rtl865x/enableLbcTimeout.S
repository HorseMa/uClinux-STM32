	.globl	enableLbcTimeout
	.ent	enableLbcTimeout
enableLbcTimeout:
	
	.set noreorder
	# get status register value
	mfc0	$8,$12
	nop
	la		$9,0xffff00ff
	and		$10,$8,$9

	# only enable LBC timeout interrupt IM6
	la		$8,0x4000
	or		$8,$8,$10
	
	# write into status register
#	mtc0	$8,$12
	nop
	nop
	jr		$31	
	nop
	.set reorder
	
	.end enableLbcTimeout
