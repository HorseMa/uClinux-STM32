
#ifndef _PCM_H
#define _PCM_H



/* ioctl commands */
enum {
	PCM_GET_MODE=0,
	PCM_GET_LOOPBACK,
	PCM_GET_TIMESLOT,
	PCM_SET_MODE,
	PCM_SET_LOOPBACK,
	PCM_SET_TIMESLOT};

/* mode definictions */
enum {
	PCM_MODE_UNIFORM=0,
	PCM_MODE_ULAW,
	PCM_MODE_ALAW};



#endif   /* _PCM_H */


