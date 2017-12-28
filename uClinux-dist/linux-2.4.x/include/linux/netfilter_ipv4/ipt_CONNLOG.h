#ifndef _IPT_CONNLOG_H_target
#define _IPT_CONNLOG_H_target

enum {
	IPT_CONNLOG_CONFIRM = (1 << 0),
	IPT_CONNLOG_DESTROY = (2 << 0),
};

struct ipt_connlog_target_info {
	u_int32_t events;
};

#endif /*_IPT_CONNLOG_H_target*/
