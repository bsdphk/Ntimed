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
 * UNIX timebase
 * =============
 *
 * Implement the timebase functions on top of a modern UNIX kernel which
 * has the some version of the Mills/Kamp kernel PLL code and either
 * [gs]ettimeofday(2) or better: clock_[gs]ettime(2) API.
 *
 */

#include <errno.h>
#include <math.h>
#include <poll.h>
#include <string.h>
#include <sys/time.h>

#include <sys/timex.h>

#include "ntimed.h"

static double adj_offset = 0;
static double adj_duration = 0;
static double adj_freq = 0;

static uintptr_t ticker;
static struct todolist *kt_tdl;

// #undef CLOCK_REALTIME		/* Test old unix code */

/**********************************************************************
 * The NTP-pll in UNIX kernels apply the offset correction in an
 * exponential-decay fashion for historical and wrong reasons.
 *
 * The short explanation is that this ends up confusing all PLLs I have
 * ever seen, by introducing mainly odd harmonics of the PLL update period
 * into all time-measurements in the system.
 *
 * A much more sane mode would be to tell the kernel "I want this much
 * offset accumulated over this many seconds", giving a constant frequency
 * over the PLL update period while still falling back to the frequency
 * estimate should the time-steering userland process fail.
 *
 * I will add such a mode to the FreeBSD kernel as a reference implementation
 * at a later date, in the mean time this code implements it by updating the
 * kernel frequency from userland as needed.
 *
 * XXX: Optimise to only wake up when truly needed, rather than every second.
 * XXX: Requires TODO cancellation.
 */

static void
kt_setfreq(struct ocx *ocx, double frequency)
{
	struct timex tx;
	int i;

	assert(isfinite(frequency));

	memset(&tx, 0, sizeof tx);
	tx.modes = MOD_STATUS;
#if defined(MOD_NANO)
	tx.modes |= MOD_NANO;
#elif defined(MOD_MICRO)
	tx.modes |= MOD_MICRO;
#endif

	tx.status = STA_PLL | STA_FREQHOLD;
	tx.modes = MOD_FREQUENCY;
	tx.freq = (long)floor(frequency * (65536 * 1e6));
	errno = 0;
	i = ntp_adjtime(&tx);
	Put(ocx, OCX_TRACE, "KERNPLL %.6e %d\n", frequency, i);
	/* XXX: what is the correct error test here ? */
	assert(i >= 0);
}

static enum todo_e __match_proto__(todo_f)
kt_ticker(struct ocx *ocx, struct todolist *tdl, void *priv)
{

	(void)ocx;
	AN(tdl);
	AZ(priv);
	kt_setfreq(ocx, adj_freq);
	ticker = 0;
	return (TODO_OK);
}

static void __match_proto__(tb_adjust_f)
kt_adjust(struct ocx *ocx, double offset, double duration, double frequency)
{
	double freq;

	(void)ocx;
	assert(duration >= 0.0);

	if (ticker)
		TODO_Cancel(kt_tdl, &ticker);

	adj_offset = offset;
	adj_duration = floor(duration);
	if (adj_offset > 0.0 && adj_duration == 0.0)
		adj_duration = 1.0;
	adj_freq = frequency;

	freq = adj_freq;
	if (adj_duration > 0.0)
		freq += adj_offset / adj_duration;
	kt_setfreq(ocx, freq);
	if (adj_duration > 0.0)
		ticker = TODO_ScheduleRel(kt_tdl, kt_ticker, NULL,
		    adj_duration, 0.0, "KT_TICK");
}

/**********************************************************************/

#ifdef CLOCK_REALTIME

static void __match_proto__(tb_step_f)
kt_step(struct ocx *ocx, double offset)
{
	double d;
	struct timespec ts;

	Put(ocx, OCX_TRACE, "KERNTIME_STEP %.3e\n", offset);
	d = floor(offset);
	offset -= d;

	AZ(clock_gettime(CLOCK_REALTIME, &ts));
	ts.tv_sec += (long)d;
	ts.tv_nsec += (long)floor(offset * 1e9);
	if (ts.tv_nsec < 0) {
		ts.tv_sec -= 1;
		ts.tv_nsec += 1000000000;
	} else if (ts.tv_nsec >= 1000000000) {
		ts.tv_sec += 1;
		ts.tv_nsec -= 1000000000;
	}
	AZ(clock_settime(CLOCK_REALTIME, &ts));
	TB_generation++;
}

#else

static void __match_proto__(tb_step_f)
kt_step(struct ocx *ocx, double offset)
{
	double d;
	struct timeval tv;

	Put(ocx, OCX_TRACE, "KERNTIME_STEP %.3e\n", offset);
	d = floor(offset);
	offset -= d;

	AZ(gettimeofday(&tv, NULL));
	tv.tv_sec += (long)d;
	tv.tv_usec += (long)floor(offset * 1e6);
	if (tv.tv_usec < 0) {
		tv.tv_sec -= 1;
		tv.tv_usec += 1000000;
	} else if (tv.tv_usec >= 1000000) {
		tv.tv_sec += 1;
		tv.tv_usec -= 1000000;
	}
	AZ(settimeofday(&tv, NULL));
	TB_generation++;
}

#endif

/**********************************************************************/

#if defined (CLOCK_REALTIME)

static struct timestamp * __match_proto__(tb_now_f)
kt_now(struct timestamp *storage)
{
	struct timespec ts;

	AZ(clock_gettime(CLOCK_REALTIME, &ts));
	return (TS_Nanosec(storage, ts.tv_sec, ts.tv_nsec));
}

#else

static struct timestamp * __match_proto__(tb_now_f)
kt_now(struct timestamp *storage)
{
	struct timeval tv;

	AZ(gettimeofday(&tv, NULL));
	return (TS_Nanosec(storage, tv.tv_sec, tv.tv_usec * 1000LL));
}

#endif

/**********************************************************************/

static int __match_proto__(tb_sleep_f)
kt_sleep(double dur)
{
	struct pollfd fds[1];
	int i;

	i = poll(fds, 0, (int)floor(dur * 1e3));
	if (i < 0 && errno == EINTR)
		return (1);
	AZ(i);
	return (0);
}

/**********************************************************************/

void
Time_Unix(struct todolist *tdl)
{

	AN(tdl);
	TB_Step = kt_step;
	TB_Adjust = kt_adjust;
	TB_Sleep = kt_sleep;
	TB_Now = kt_now;
	kt_tdl = tdl;

	/* XXX: test if we have perms */
}

/**********************************************************************
 * Non-tweaking subset.
 */

void
Time_Unix_Passive(void)
{

	TB_Sleep = kt_sleep;
	TB_Now = kt_now;
}
