#ifndef _REQUEST_LIST_H
#define _REQUEST_LIST_H
/*
 * request_list.h	Hide the handling of the REQUEST list from
 *			the main server.
 *
 * Version:	$Id: request_list.h,v 1.9 2004/05/28 14:42:27 aland Exp $
 *
 */

extern int rl_init(void);
extern void rl_delete(REQUEST *request);
extern void rl_add(REQUEST *request);
extern REQUEST *rl_find(RADIUS_PACKET *packet);
extern int rl_add_proxy(REQUEST *request);
extern REQUEST *rl_find_proxy(RADIUS_PACKET *packet);
extern REQUEST *rl_next(REQUEST *request);
extern int rl_num_requests(void);

#define RL_WALK_CONTINUE (0)
#define RL_WALK_STOP     (-1)

typedef int (*RL_WALK_FUNC)(REQUEST *, void *);

extern int rl_walk(RL_WALK_FUNC walker, void *data);
extern struct timeval *rl_clean_list(time_t now);

#endif /* _REQUEST_LIST_H */
