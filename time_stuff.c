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
 * Timebase infrastructure
 * =======================
 *
 * This file implements the generic timebase stuff, calling out to a specific
 * implementation through the function pointers as required.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ntimed.h"

#define NANO_FRAC	18446744074ULL		// 2^64 / 1e9

/*
 * Whenever the clock is stepped, we increment this generation number.
 *
 * XXX: Add support for stepping externally via a signal (SIGRESUME ?)
 */
int TB_generation = 41;

/**********************************************************************/

static struct timestamp *
ts_fixstorage(struct timestamp *storage)
{
	if (storage == NULL) {
		ALLOC_OBJ(storage, TIMESTAMP_MAGIC);
		AN(storage);
	} else {
		AN(storage);
		memset(storage, 0, sizeof *storage);
		storage->magic = TIMESTAMP_MAGIC;
	}
	return (storage);
}

/**********************************************************************/

struct timestamp *
TS_Nanosec(struct timestamp *storage, int64_t sec, int64_t nsec)
{

	storage = ts_fixstorage(storage);

	assert(sec >= 0);
	assert(nsec >= 0);
	assert(nsec < 1000000000);
	storage->sec = (uint64_t)sec;
	storage->frac = (uint32_t)nsec * NANO_FRAC;
	return (storage);
}

/**********************************************************************/

struct timestamp *
TS_Double(struct timestamp *storage, double d)
{

	assert(d >= 0.0);
	storage = ts_fixstorage(storage);

	storage->sec += (uint64_t)floor(d);
	d -= floor(d);
	storage->frac = (uint64_t)ldexp(d, 64);
	return (storage);
}

/**********************************************************************/

void
TS_Add(struct timestamp *ts, double dt)
{
	double di;

	CHECK_OBJ_NOTNULL(ts, TIMESTAMP_MAGIC);
	dt += ldexp(ts->frac, -64);
	di = floor(dt);
	ts->sec += (uint64_t)di;
	ts->frac = (uint64_t)ldexp(dt - di, 64);
}

/**********************************************************************/

double
TS_Diff(const struct timestamp *t1, const struct timestamp *t2)
{
	double d;

	CHECK_OBJ_NOTNULL(t1, TIMESTAMP_MAGIC);
	CHECK_OBJ_NOTNULL(t2, TIMESTAMP_MAGIC);
	d = ldexp((double)t1->frac - (double)t2->frac, -64);
	d += ((double)t1->sec - (double)t2->sec);

	return (d);
}

/**********************************************************************/

int
TS_SleepUntil(const struct timestamp *t)
{
	struct timestamp now;
	double dt;

	TB_Now(&now);
	dt = TS_Diff(t, &now);
	if (dt <= 0.)
		return (0);
	return (TB_Sleep(dt));
}

/**********************************************************************/

void
TS_Format(char *buf, size_t len, const struct timestamp *ts)
{
	CHECK_OBJ_NOTNULL(ts, TIMESTAMP_MAGIC);
	uint64_t x, y;
	int i;

	/* XXX: Nanosecond precision is enough for everybody. */
	x = ts->sec;
	y = (ts->frac + NANO_FRAC / 2ULL) / NANO_FRAC;
	if (y >= 1000000000ULL) {
		y -= 1000000000ULL;
		x += 1;
	}
	i = snprintf(buf, len, "%jd.%09jd", (intmax_t)x, (intmax_t)y);
	assert(i < (int)len);
}

/**********************************************************************
 * DUMMY TimeBase functions
 */

static struct timestamp *
tb_Now(struct timestamp *storage)
{

	(void)storage;
	WRONG("No TB_Now");
	NEEDLESS_RETURN(NULL);
}

tb_now_f *TB_Now = tb_Now;

/**********************************************************************/

static int
tb_Sleep(double dur)
{
	(void)dur;
	WRONG("No TB_Sleep");
	NEEDLESS_RETURN(-1);
}

tb_sleep_f *TB_Sleep = tb_Sleep;

/**********************************************************************/

static void __match_proto__(tb_step_f)
tb_Step(struct ocx *ocx, double offset)
{
	(void)ocx;
	(void)offset;
	WRONG("No TB_Step");
}

tb_step_f *TB_Step = tb_Step;

/**********************************************************************/

static void __match_proto__(tb_adjust_f)
tb_Adjust(struct ocx *ocx, double offset, double duration, double frequency)
{
	(void)ocx;
	(void)offset;
	(void)duration;
	(void)frequency;
	WRONG("No TB_Adjust");
}

tb_adjust_f *TB_Adjust = tb_Adjust;

/**********************************************************************
 * Timebase test functions.
 */

static int
ts_onetest(struct ocx *ocx, const struct timestamp *ts, double off)
{
	struct timestamp ts2;
	double dt;
	char buf[40];

	TS_Format(buf, sizeof buf, ts);
	ts2 = *ts;
	TS_Add(&ts2, off);
	Debug(ocx, "%s + %12.9f = ", buf, off);
	TS_Format(buf, sizeof buf, &ts2);
	dt = TS_Diff(&ts2, ts) - off;
	Debug(ocx, "%s %8.1e", buf, dt);
	if (fabs(dt) > 5e-10) {
		Debug(ocx, " ERR\n");
		return (1);
	}
	Debug(ocx, " OK\n");
	return (0);
}

void
TS_RunTest(struct ocx *ocx)
{
	struct timestamp ts;
	int nf = 0;

	TB_Now(&ts);
	nf += ts_onetest(ocx, &ts, 1e-9);
	nf += ts_onetest(ocx, &ts, 1e-8);
	nf += ts_onetest(ocx, &ts, 1e-6);
	nf += ts_onetest(ocx, &ts, 1e-3);
	nf += ts_onetest(ocx, &ts, 1e-1);
	nf += ts_onetest(ocx, &ts, 0.999);
	nf += ts_onetest(ocx, &ts, 1.001);
	nf += ts_onetest(ocx, &ts, 1.999);
	nf += ts_onetest(ocx, &ts, -2.000);
	nf += ts_onetest(ocx, &ts, -1.999);
	nf += ts_onetest(ocx, &ts, -1.000);
	nf += ts_onetest(ocx, &ts, -0.999);
	nf += ts_onetest(ocx, &ts, -1e-3);
	nf += ts_onetest(ocx, &ts, -1e-6);
	nf += ts_onetest(ocx, &ts, -1e-9);
	Debug(ocx, "TS_RunTest: %d failures\n", nf);
	AZ(nf);
}


