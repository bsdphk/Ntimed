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
 * Source Combiner based on delta-pdfs
 * ===================================
 *
 * The basic principle here is that sources gives us four values:
 *   - The highest low value were the probability is zero.
 *   - The lowest high value were the probability is zero.
 *   - The most probable value
 *   - The relative trust in that value [0...1]
 * Together this defines a triangular probability density function.
 *
 * The combiner adds all these pdfs' together weighted by trust
 * and finds the highest probability which sports a quorum.
 *
 * See also: http://phk.freebsd.dk/time/20141107.html
 *
 * XXX: decay trust by age
 * XXX: auto determine quorom if param not set
 * XXX: param for minimum probability density
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ntimed.h"


struct cd_stat {
	double				x;
	double				prob;
	unsigned			quorum;
};

struct cd_source {
	unsigned			magic;
#define CD_SOURCE_MAGIC			0x2799775c
	TAILQ_ENTRY(cd_source)		list;

	struct combiner			combiner;

	struct combine_delta		*cd;
	double				trust, low, mid, high;
	int				tb_gen;
};

struct combine_delta {
	unsigned			magic;
#define COMBINE_DELTA_MAGIC		0x8dc5030c

	unsigned			nsrc;
	TAILQ_HEAD(, cd_source)		head;
};

struct combine_delta *
CD_New(void)
{
	struct combine_delta *cd;

	ALLOC_OBJ(cd, COMBINE_DELTA_MAGIC);
	AN(cd);

	TAILQ_INIT(&cd->head);
	return (cd);
}

static void
cd_try_peak(const struct combine_delta *cd, double *mx, double *my, double x,
    struct cd_stat *st)
{
	struct cd_source *cs;

	// XXX: Hack to make plots with log zscale and only one
	// XXX: source look sensible.
	st->x = x;
	st->prob = 0.001;
	st->quorum = 0;

	TAILQ_FOREACH(cs, &cd->head, list) {
		if (cs->tb_gen != TB_generation)
			continue;
		if (x < cs->low)
			continue;
		if (x > cs->high)
			continue;
		if (cs->low >= cs->high)
			continue;

		st->quorum++;
		if (x < cs->mid) {
			st->prob += cs->trust * 2.0 * (x - cs->low) /
			    ((cs->high - cs->low) * (cs->mid - cs->low));
		} else {
			st->prob += cs->trust * 2.0 * (cs->high - x) /
			    ((cs->high - cs->low) * (cs->high - cs->mid));
		}
		if (isnan(st->prob)) {
			Fail(NULL, 0, "lo %.3e hi %.3e mid %.3e",
			    cs->low, cs->high, cs->mid);
		}
	}
	if (st->prob > *my) {
		*my = st->prob;
		*mx = x;
	}
}

static int
stat_cmp(const void *p1, const void *p2)
{
	const struct cd_stat *left = p1;
	const struct cd_stat *right = p2;

	/*lint -save -e514 */
	return ((left->x > right->x) - (left->x < right->x));
	/*lint -restore */
}

static void
cd_find_peak(struct ocx *ocx, const struct combine_delta *cd)
{
	struct cd_source *cs;
	double max_x = 0;
	double max_y = 1;
	struct cd_stat st[cd->nsrc * 3L];
	size_t m;

	m = 0;
	TAILQ_FOREACH(cs, &cd->head, list) {
		if (cs->tb_gen != TB_generation)
			continue;
		cd_try_peak(cd, &max_x, &max_y, cs->low, &st[m++]);
		cd_try_peak(cd, &max_x, &max_y, cs->mid, &st[m++]);
		cd_try_peak(cd, &max_x, &max_y, cs->high, &st[m++]);
	}
	Put(ocx, OCX_TRACE,
	    " %.3e %.3e %.3e\n", max_x, max_y, log(max_y)/log(10.));
	PLL(ocx, max_x, max_y);
	qsort(st, cd->nsrc * 3L, sizeof st[0], stat_cmp);
}

static void __match_proto__(combine_f)
cd_filter(struct ocx *ocx, const struct combiner *cb,
    double trust, double low, double mid, double high)
{
	struct cd_source *cs;
	struct combine_delta *cd;

	CHECK_OBJ_NOTNULL(cb, COMBINER_MAGIC);
	CAST_OBJ_NOTNULL(cs, cb->priv, CD_SOURCE_MAGIC);
	cd = cs->cd;
	CHECK_OBJ_NOTNULL(cd, COMBINE_DELTA_MAGIC);

	/* Sign: local - remote -> postive is ahead */
	assert(trust >= 0 && trust <= 1.0);
	cs->trust = trust;
	cs->low = low;
	cs->mid = mid;
	cs->high = high;
	cs->tb_gen = TB_generation;

	Put(ocx, OCX_TRACE,
	    "Combine %s %s %.6f %.6f %.6f", cb->name1, cb->name2,
	    cs->low, cs->mid, cs->high);

	cd_find_peak(ocx, cd);
}

struct combiner *
CD_AddSource(struct combine_delta *cd, const char *name1, const char *name2)
{
	struct cd_source *cs;

	CHECK_OBJ_NOTNULL(cd, COMBINE_DELTA_MAGIC);

	ALLOC_OBJ(cs, CD_SOURCE_MAGIC);
	AN(cs);
	cs->cd = cd;
	cs->low = cs->mid = cs->high = nan("");
	TAILQ_INSERT_TAIL(&cd->head, cs, list);

	INIT_OBJ(&cs->combiner, COMBINER_MAGIC);
	cs->combiner.func = cd_filter;
	cs->combiner.priv = cs;
	cs->combiner.name1 = name1;
	cs->combiner.name2 = name2;

	cd->nsrc += 1;

	return (&cs->combiner);
}
