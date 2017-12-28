#ifndef __CLE_CMD_H__
#define __CLE_CMD_H__

#ifdef CONFIG_RTL865XB_EXP_PERFORMANCE_EVALUATION
	void   clearCOP3Counters(void);
	void   startCOP3Counters(int32 countInst);
	uint32 stopCOP3Counters(void);
	void   DisplayCOP3Counters(void);
	void   pauseCOP3Counters(void);
#endif

#if CONFIG_RTL865X_MODULE_ROMEDRV

	void rtl8651_8185flashCfg(int8 *cmd, uint32 cmdLen);
	
#if	defined(CONFIG_RTL865X_CLE)
	void __init rtlcle_init (void);
	void __exit rtlcle_exit(void);
#endif
 
	int simple_init(void);
	void simple_cleanup(void);
#endif

#endif
