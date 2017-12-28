#ifndef _EAP_MSCHAPV2_H
#define _EAP_MSCHAPV2_H

#include "eap.h"

#define PW_EAP_MSCHAPV2_ACK		0
#define PW_EAP_MSCHAPV2_CHALLENGE	1
#define PW_EAP_MSCHAPV2_RESPONSE	2
#define PW_EAP_MSCHAPV2_SUCCESS		3
#define PW_EAP_MSCHAPV2_FAILURE		4
#define PW_EAP_MSCHAPV2_MAX_CODES	4

#define MSCHAPV2_HEADER_LEN 	5
#define MSCHAPV2_CHALLENGE_LEN  16
#define MSCHAPV2_RESPONSE_LEN  50

#define MSCHAPV2_FAILURE_MESSAGE "E=691 R=0"
#define MSCHAPV2_FAILURE_MESSAGE_LEN 9
typedef struct mschapv2_header_t {
	uint8_t opcode;
	uint8_t mschapv2_id;
	uint8_t ms_length[2];
	uint8_t value_size;
} mschapv2_header_t;

typedef struct mschapv2_opaque_t {
	int		code;
	uint8_t		challenge[MSCHAPV2_CHALLENGE_LEN];
} mschapv2_opaque_t;

#endif /*_EAP_MSCHAPV2_H*/
