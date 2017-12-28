/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: ntservice.h,v 1.2 2002/08/03 01:31:48 mayer Exp $ */

#ifndef NTSERVICE_H
#define NTSERVICE_H

#include <winsvc.h>

#define NTP_DISPLAY_NAME "NetworkTimeProtocol"
#define NTP_SERVICE_NAME "ntpd"

void ntservice_init();
void UpdateSCM(DWORD);
void ServiceControl(DWORD dwCtrlCode);
void ntservice_shutdown();
BOOL ntservice_isservice();
BOOL WINAPI OnConsoleEvent(DWORD dwCtrlType);

#endif