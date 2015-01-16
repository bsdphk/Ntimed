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
 * NTP packet (de)serialization
 * ============================
 *
 *      0                   1                   2                   3
 *      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  0  |LI | VN  |Mode |    Stratum     |     Poll      |  Precision   |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  4  |                         Root Delay                            |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  8  |                         Root Dispersion                       |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 12  |                          Reference ID                         |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 16  |                                                               |
 *     +                     Reference Timestamp (64)                  +
 *     |                                                               |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 24  |                                                               |
 *     +                      Origin Timestamp (64)                    +
 *     |                                                               |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 32  |                                                               |
 *     +                      Receive Timestamp (64)                   +
 *     |                                                               |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 40  |                                                               |
 *     +                      Transmit Timestamp (64)                  +
 *     |                                                               |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 */

#include <stdlib.h>
#include <string.h>

#include "ntimed.h"
#include "ntp.h"
#include "ntimed_endian.h"

/*
 * Seconds between 1900 (NTP epoch) and 1970 (UNIX epoch).
 * 17 is the number of leapdays.
 */
#define NTP_UNIX        (((1970U - 1900U) * 365U + 17U) * 24U * 60U * 60U)

/**********************************************************************
 * Picking a NTP packet apart in a safe, byte-order agnostic manner
 */

static void
ntp64_2ts(struct timestamp *ts, const uint8_t *ptr)
{

	INIT_OBJ(ts, TIMESTAMP_MAGIC);
	ts->sec = Be32dec(ptr) - NTP_UNIX;
	ts->frac = (uint64_t)Be32dec(ptr + 4) << 32ULL;
}

static void
ntp32_2ts(struct timestamp *ts, const uint8_t *ptr)
{

	INIT_OBJ(ts, TIMESTAMP_MAGIC);
	ts->sec = Be16dec(ptr);
	ts->frac = (uint64_t)Be16dec(ptr + 2) << 48ULL;
}


struct ntp_packet *
NTP_Packet_Unpack(struct ntp_packet *np, void *ptr, ssize_t len)
{
	uint8_t *p = ptr;

	AN(ptr);
	if (len != 48) {
		/* XXX: Diagnostic */
		return (NULL);
	}

	if (np == NULL) {
		ALLOC_OBJ(np, NTP_PACKET_MAGIC);
		AN(np);
	} else {
		INIT_OBJ(np, NTP_PACKET_MAGIC);
	}

	np->ntp_leap = (enum ntp_leap)(p[0] >> 6);
	np->ntp_version = (p[0] >> 3) & 0x7;
	np->ntp_mode = (enum ntp_mode)(p[0] & 0x07);
	np->ntp_stratum = p[1];
	np->ntp_poll = p[2];
	np->ntp_precision = (int8_t)p[3];
	ntp32_2ts(&np->ntp_delay, p + 4);
	ntp32_2ts(&np->ntp_dispersion, p + 8);
	memcpy(np->ntp_refid, p + 12, 4L);
	ntp64_2ts(&np->ntp_reference, p + 16);
	ntp64_2ts(&np->ntp_origin, p + 24);
	ntp64_2ts(&np->ntp_receive, p + 32);
	ntp64_2ts(&np->ntp_transmit, p + 40);
	return (np);
}

/**********************************************************************
 * Putting a NTP packet apart in a safe, byte-order agnostic manner
 */

static void
ts_2ntp32(uint8_t *dst, const struct timestamp *ts)
{

	CHECK_OBJ_NOTNULL(ts, TIMESTAMP_MAGIC);
	assert(ts->sec < 65536);
	Be16enc(dst, (uint16_t)ts->sec);
	Be16enc(dst + 2, ts->frac >> 48ULL);
}

static void
ts_2ntp64(uint8_t *dst, const struct timestamp *ts)
{

	CHECK_OBJ_NOTNULL(ts, TIMESTAMP_MAGIC);
	Be32enc(dst, ts->sec + NTP_UNIX);
	Be32enc(dst + 4, ts->frac >> 32ULL);
}

size_t
NTP_Packet_Pack(void *ptr, ssize_t len, struct ntp_packet *np)
{
	uint8_t *pbuf = ptr;

	AN(ptr);
	assert(len >= 48);
	CHECK_OBJ_NOTNULL(np, NTP_PACKET_MAGIC);
	assert(np->ntp_version < 8);
	assert(np->ntp_stratum < 15);

	pbuf[0] = (uint8_t)np->ntp_leap;
	pbuf[0] <<= 3;
	pbuf[0] |= np->ntp_version;
	pbuf[0] <<= 3;
	pbuf[0] |= (uint8_t)np->ntp_mode;
	pbuf[1] = np->ntp_stratum;
	pbuf[2] = np->ntp_poll;
	pbuf[3] = (uint8_t)np->ntp_precision;
	ts_2ntp32(pbuf + 4, &np->ntp_delay);
	ts_2ntp32(pbuf + 8, &np->ntp_dispersion);
	memcpy(pbuf + 12, np->ntp_refid, 4L);
	ts_2ntp64(pbuf + 16, &np->ntp_reference);
	ts_2ntp64(pbuf + 24, &np->ntp_origin);
	ts_2ntp64(pbuf + 32, &np->ntp_receive);

	TB_Now(&np->ntp_transmit);
	ts_2ntp64(pbuf + 40, &np->ntp_transmit);

	/* Reverse again, to avoid subsequent trouble from rounding. */
	ntp64_2ts(&np->ntp_transmit, pbuf + 40);

	return (48);
}
