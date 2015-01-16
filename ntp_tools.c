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
 * NTP tools
 * =========
 *
 *
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "ntimed.h"
#include "ntp.h"
#include "ntimed_endian.h"

/**********************************************************************
 * Build a standard client query packet
 */

void
NTP_Tool_Client_Req(struct ntp_packet *np)
{
	AN(np);
	INIT_OBJ(np, NTP_PACKET_MAGIC);

	np->ntp_leap = NTP_LEAP_UNKNOWN;
	np->ntp_version = 4;
	np->ntp_mode = NTP_MODE_CLIENT;
	np->ntp_stratum = 0;
	np->ntp_poll = 4;
	np->ntp_precision = -6;
	INIT_OBJ(&np->ntp_delay, TIMESTAMP_MAGIC);
	np->ntp_delay.sec = 1;
	INIT_OBJ(&np->ntp_dispersion, TIMESTAMP_MAGIC);
	np->ntp_dispersion.sec = 1;
	INIT_OBJ(&np->ntp_reference, TIMESTAMP_MAGIC);
	INIT_OBJ(&np->ntp_origin, TIMESTAMP_MAGIC);
	INIT_OBJ(&np->ntp_receive, TIMESTAMP_MAGIC);
}

/**********************************************************************
 * Format a NTP packet in a standardized layout for subsequent parsing.
 *
 * We dump absolute timestamps relative to the origin timestamp.
 *
 * XXX: Nanosecond precision is enough for everybody.
 */

static void __printflike(3, 4)
bxprintf(char **bp, const char *e, const char *fmt, ...)
{
	va_list ap;

	assert(*bp < e);
	va_start(ap, fmt);
	*bp += vsnprintf(*bp, (unsigned)(e - *bp), fmt, ap);
	va_end(ap);
}

void
NTP_Tool_Format(char *p, ssize_t len, const struct ntp_packet *pkt)
{
	char *e;
	char buf[40];

	AN(p);
	assert(len > 0);
	CHECK_OBJ_NOTNULL(pkt, NTP_PACKET_MAGIC);

	e = p + len;

	bxprintf(&p, e, "[%d", pkt->ntp_leap);
	bxprintf(&p, e, " %u", pkt->ntp_version);

	bxprintf(&p, e, " %d", pkt->ntp_mode);

	bxprintf(&p, e, " %3u", pkt->ntp_stratum);

	bxprintf(&p, e, " %3u", pkt->ntp_poll);

	bxprintf(&p, e, " %4d", pkt->ntp_precision);

	TS_Format(buf, sizeof buf, &pkt->ntp_delay);
	bxprintf(&p, e, " %s", buf); assert(p < e);

	TS_Format(buf, sizeof buf, &pkt->ntp_dispersion);
	bxprintf(&p, e, " %s", buf); assert(p < e);

	bxprintf(&p, e, " 0x%02x%02x%02x%02x",
	    pkt->ntp_refid[0], pkt->ntp_refid[1],
	    pkt->ntp_refid[2], pkt->ntp_refid[3]);

	bxprintf(&p, e, " %.9f",
	    TS_Diff(&pkt->ntp_reference, &pkt->ntp_origin));

	TS_Format(buf, sizeof buf, &pkt->ntp_origin);
	bxprintf(&p, e, " %s", buf); assert(p < e);

	bxprintf(&p, e, " %.9f",
	    TS_Diff(&pkt->ntp_receive, &pkt->ntp_origin));

	bxprintf(&p, e, " %.9f",
	    TS_Diff(&pkt->ntp_transmit, &pkt->ntp_receive));

	if (pkt->ts_rx.sec && pkt->ts_rx.frac) {
		bxprintf(&p, e, " %.9f]",
		    TS_Diff(&pkt->ts_rx, &pkt->ntp_transmit));
	} else {
		bxprintf(&p, e, " %.9f]", 0.0);
	}
	assert(p < e);
}

/**********************************************************************
 * Scan a packet in NTP_Tool_Format layout.
 */

int
NTP_Tool_Scan(struct ntp_packet *pkt, const char *buf)
{
	unsigned u_fields[8];
	double d_fields[7];
	char cc;
	int i;

	i = sscanf(buf,
	    "[%u %u %u %u %u %lf %lf %lf %x %lf %u.%u %lf %lf %lf%c",
	    u_fields + 0,	/* NTP_leap */
	    u_fields + 1,	/* NTP_version */
	    u_fields + 2,	/* NTP_mode */
	    u_fields + 3,	/* NTP_stratum */
	    u_fields + 4,	/* NTP_poll */
	    d_fields + 0,	/* NTP_precision */
	    d_fields + 1,	/* NTP_delay */
	    d_fields + 2,	/* NTP_dispersion */
	    u_fields + 5,	/* NTP_refid */
	    d_fields + 3,	/* NTP_reference - NTP_origin */
	    u_fields + 6,	/* NTP_origin:sec */
	    u_fields + 7,	/* NTP_origin:nsec */
	    d_fields + 4,	/* NTP_receive - NTP_origin */
	    d_fields + 5,	/* NTP_transmit - NTP_receive */
	    d_fields + 6,	/* ts_rx - NTP_transmit*/
	    &cc);
	if (i != 16 || cc != ']')
		return (-1);

	INIT_OBJ(pkt, NTP_PACKET_MAGIC);
	pkt->ntp_leap = (enum ntp_leap)u_fields[0];
	pkt->ntp_version = (uint8_t)u_fields[1];
	pkt->ntp_mode = (enum ntp_mode)u_fields[2];
	pkt->ntp_stratum = (uint8_t)u_fields[3];
	pkt->ntp_poll = (uint8_t)u_fields[4];
	pkt->ntp_precision = (int8_t)floor(d_fields[0]);
	TS_Double(&pkt->ntp_delay, d_fields[1]);
	TS_Double(&pkt->ntp_dispersion, d_fields[2]);
	Be32enc(pkt->ntp_refid, u_fields[5]);

	TS_Nanosec(&pkt->ntp_origin, u_fields[6], u_fields[7]);

	pkt->ntp_reference = pkt->ntp_origin;
	TS_Add(&pkt->ntp_reference, d_fields[3]);

	pkt->ntp_receive = pkt->ntp_origin;
	TS_Add(&pkt->ntp_receive, d_fields[4]);

	pkt->ntp_transmit = pkt->ntp_receive;
	TS_Add(&pkt->ntp_transmit, d_fields[5]);

	if (d_fields[6] != 0.0) {
		pkt->ts_rx = pkt->ntp_transmit;
		TS_Add(&pkt->ts_rx, d_fields[6]);
	} else
		INIT_OBJ(&pkt->ts_rx, TIMESTAMP_MAGIC);
	return (0);
}
