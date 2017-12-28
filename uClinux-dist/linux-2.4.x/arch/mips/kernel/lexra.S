#/*-------------------------------------------------------------------
#**
#** void lx4180_WriteCCTL(uint32)
#**
#**-------------------------------------------------------------------
#*/
	.globl	lx4180_writeCacheCtrl
	.ent	lx4180_writeCacheCtrl
lx4180_writeCacheCtrl:
	mtc0	$4,$20
	jr		$31	
	nop
	.end lx4180_writeCacheCtrl