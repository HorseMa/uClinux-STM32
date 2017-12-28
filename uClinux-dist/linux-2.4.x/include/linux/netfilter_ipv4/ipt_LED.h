#ifndef _IPT_LED_H
#define _IPT_LED_H

enum {
	IPT_LED_SET = 0,
	IPT_LED_SAVE,
	IPT_LED_RESTORE,
};

struct ipt_led_info {
	u_int32_t led;
	u_int8_t mode;
};

#endif /*_IPT_LED_H*/
