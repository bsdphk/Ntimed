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
 * Filter incoming NTP packets
 * ===========================
 */

#include <math.h>
#include <stdlib.h>

#include "ntimed.h"

#include "ntp.h"

#define PARAM_NTP_FILTER PARAM_INSTANCE
#define PARAM_TABLE_NAME ntp_filter_param_table
#include "param_instance.h"
#undef PARAM_TABLE_NAME
#undef PARAM_NTP_FILTER

struct ntp_filter {
	unsigned		magic;
#define NTP_FILTER_MAGIC	0xf7b7032d

	double			lo, mid, hi;
	double			alo, amid, ahi;
	double			alolo, ahihi;
	double			navg;
	double			trust;

	int			generation;
};

static void __match_proto__(ntp_filter_f)
nf_filter(struct ocx *ocx, const struct ntp_peer *np)
{
	struct ntp_filter *nf;
	struct ntp_packet *rxp;
	int branch, fail_hi, fail_lo;
	double lo_noise, hi_noise;
	double lo_lim, hi_lim;
	double r;
	char buf[256];

	CAST_OBJ_NOTNULL(nf, np->filter_priv, NTP_FILTER_MAGIC);

	if (nf->generation != TB_generation) {
		nf->navg = 0;
		nf->alo = nf->amid = nf->ahi = 0.0;
		nf->alolo = nf->ahihi = 0.0;
	}

	rxp = np->rx_pkt;
	CHECK_OBJ_NOTNULL(rxp, NTP_PACKET_MAGIC);

	NTP_Tool_Format(buf, sizeof buf, rxp);

	Put(NULL, OCX_TRACE, "NTP_Packet %s %s %s\n",
	    np->hostname, np->ip, buf);

	if (rxp->ntp_leap == NTP_LEAP_UNKNOWN)
		return;		// XXX diags

	// XXX: Check leap warnings in wrong months
	// XXX: Check leap warnings against other sources

	if (rxp->ntp_version < 3 || rxp->ntp_version > 4) {
		Put(ocx, OCX_TRACE, "NF Bad version %d\n", rxp->ntp_version);
		return;
	}

	if (rxp->ntp_mode != NTP_MODE_SERVER) {
		Put(ocx, OCX_TRACE, "NF Bad mode %d\n", rxp->ntp_mode);
		return;
	}

	if (rxp->ntp_stratum == 0 || rxp->ntp_stratum > 15) {
		Put(ocx, OCX_TRACE, "NF Bad stratum %d\n", rxp->ntp_stratum);
		return;
	}

	r = TS_Diff(&rxp->ntp_transmit, &rxp->ntp_receive);
	if (r <= 0.0) {
		Put(ocx, OCX_TRACE, "NF rx after tx %.3e\n", r);
		return;
	}

	r = TS_Diff(&rxp->ntp_transmit, &rxp->ntp_reference);
	if (r < -2e-9) {
		/* two nanoseconds to Finagle rounding errors */
		Put(ocx, OCX_TRACE, "NF ref after tx %.3e\n", r);
		return;		// XXX diags
	}

	// This is almost never a good sign.
	if (r > 2048) {
		/* XXX: 2048 -> param */
		Put(ocx, OCX_TRACE, "NF ancient ref %.3e\n", r);
		return;
	}

	if (nf->navg < param_ntp_filter_average)
		nf->navg += 1;

	nf->lo = TS_Diff(&rxp->ntp_origin, &rxp->ntp_receive);
	nf->hi = TS_Diff(&rxp->ts_rx, &rxp->ntp_transmit);
	nf->mid = .5 * (nf->lo + nf->hi);

	if (nf->navg > 2) {
		lo_noise = sqrt(nf->alolo - nf->alo * nf->alo);
		hi_noise = sqrt(nf->ahihi - nf->ahi * nf->ahi);
	} else {
		lo_noise = 0.0;
		hi_noise = 0.0;
	}

	lo_lim = nf->alo - lo_noise * param_ntp_filter_threshold;
	hi_lim = nf->ahi + hi_noise * param_ntp_filter_threshold;

	fail_lo = nf->lo < lo_lim;
	fail_hi = nf->hi > hi_lim;

	if (fail_lo && fail_hi) {
		branch = 1;
	} else if (nf->navg > 3 && fail_lo) {
		nf->mid = nf->amid + (nf->hi - nf->ahi);
		branch = 2;
	} else if (nf->navg > 3 && fail_hi) {
		nf->mid = nf->amid + nf->lo - nf->alo;
		branch = 3;
	} else {
		branch = 4;
	}

	r = nf->navg;
	if (nf->navg > 2 && branch != 4)
		r *= r;

	nf->alo += (nf->lo - nf->alo) / r;
	nf->amid += (nf->mid - nf->amid) / r;
	nf->ahi += (nf->hi - nf->ahi) / r;
	nf->alolo += (nf->lo*nf->lo - nf->alolo) / r;
	nf->ahihi += (nf->hi*nf->hi - nf->ahihi) / r;

	if (rxp->ntp_stratum == 0)
		nf->trust = 0.0;
	else if (rxp->ntp_stratum == 15)
		nf->trust = 0.0;
	else
		nf->trust = 1.0 / rxp->ntp_stratum;

	Put(ocx, OCX_TRACE,
	    "NTP_Filter %s %s %d %.3e %.3e %.3e %.3e %.3e %.3e\n",
	    np->hostname, np->ip, branch,
	    nf->lo, nf->mid, nf->hi,
	    lo_lim, nf->amid, hi_lim);

	if (np->combiner->func != NULL)
		np->combiner->func(ocx, np->combiner,
		    nf->trust, nf->lo, nf->mid, nf->hi);
}

void
NF_New(struct ntp_peer *np)
{
	struct ntp_filter *nf;

	ALLOC_OBJ(nf, NTP_FILTER_MAGIC);
	AN(nf);
	np->filter_func = nf_filter;
	np->filter_priv = nf;
}

void
NF_Init(void)
{
	Param_Register(ntp_filter_param_table);
}
