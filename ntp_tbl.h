/*-
 * Copyright (c) 2014 Poul-Henning Kamp
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Table generator file for tables related to the NTP protocol.
 */

/*lint -save -e525 -e539 */
#ifdef NTP_MODE
NTP_MODE(0,	mode0,		MODE0)
NTP_MODE(1,	symact,		SYMACT)
NTP_MODE(2,	sympas,		SYMPAS)
NTP_MODE(3,	client,		CLIENT)
NTP_MODE(4,	server,		SERVER)
NTP_MODE(5,	bcast,		BCAST)
NTP_MODE(6,	ctrl,		CTRL)
NTP_MODE(7,	mode7,		MODE7)
#endif

#ifdef NTP_LEAP
NTP_LEAP(0,	none,		NONE)
NTP_LEAP(1,	ins,		INS)
NTP_LEAP(2,	del,		DEL)
NTP_LEAP(3,	unknown,	UNKNOWN)
#endif

#ifdef NTP_STATE
NTP_STATE(0,	new,		NEW,		"Newly configured peer.")
NTP_STATE(1,	active,		ACTIVE,		"Good (and used) peer.")
NTP_STATE(2,	available,	AVAILABLE,	"Not good enough peer.")
NTP_STATE(3,	unsynchronized,	UNSYNCHRONIZED, "Bad peer.")
NTP_STATE(4,	unresponsive,	UNRESPONSIVE,	"Peer does not respond.")
NTP_STATE(5,	unreachable,	UNREACHABLE,	"Peer cannot be reached.")
NTP_STATE(6,	multihome,	MULTIHOME,	"Copy of multihomed peer.")
NTP_STATE(7,	duplicate,	DUPLICATE,	"Duplicate peer.")
#endif

/*lint -restore */
