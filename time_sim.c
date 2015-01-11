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
 * Simulated timebase
 * ==================
 *
 * Very simple minded:  Time advances when TB_Sleep() is called only.
 *
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ntimed.h"

static struct timestamp st_now;

static double freq = 0;
static double freq0 = 0;

static double adj_offset = 0;
static double adj_duration = 0;
static double adj_freq = 0;

/*
 * This variable is public and represent the amount of time the simulated
 * clock has been tweaked by the TB_Step() and TB_Adjust() functions.
 *
 * This can be used to "(re-)model" previously recorded event series
 * onto the ST timebase.
 */

double Time_Sim_delta = 0;

/**********************************************************************/

static struct timestamp *
st_Now(struct timestamp *storage)
{
	if (storage == NULL)
		ALLOC_OBJ(storage, TIMESTAMP_MAGIC);
	AN(storage);
	*storage = st_now;
	return (storage);
}

/**********************************************************************/

static int
st_Sleep(double dur)
{

	TS_Add(&st_now, dur);
	Time_Sim_delta += dur * freq;
	return (0);
}

/**********************************************************************/

static void __match_proto__(tb_step_f)
st_Step(struct ocx *ocx, double offset)
{

	Debug(ocx, "SIMSTEP %.3e\n", offset);
	Time_Sim_delta += offset;
	TB_generation++;
}

/**********************************************************************/

static void __match_proto__(tb_adjust_f)
st_Adjust(struct ocx *ocx, double offset, double duration, double frequency)
{

	(void)ocx;
	adj_offset = offset;
	adj_duration = floor(duration);
	if (adj_offset > 0.0 && adj_duration == 0.0)
		adj_duration = 1.0;
	adj_freq = frequency;
}

/**********************************************************************/

static enum todo_e __match_proto__(todo_f)
st_kern_pll(struct ocx *ocx, struct todolist *tdl, void *priv)
{
	double d;

	(void)ocx;
	AN(tdl);
	AZ(priv);
	freq = freq0 + adj_freq;
	if (adj_duration > 0.0) {
		d = adj_offset / adj_duration;
		freq += d;
		adj_offset -= d;
		adj_duration -= 1.0;
	}
	Put(ocx, OCX_TRACE, "SIMPLL %.3e %.3e %.3e\n",
	    adj_freq, adj_offset, adj_duration);
	return (TODO_OK);
}

/**********************************************************************
 * Mechanism to artificially bump simulated clock around.
 */

struct st_bump {
	unsigned		magic;
#define ST_BUMP_MAGIC		0xc8981be3
	double			bfreq;
	double			bphase;
};

static enum todo_e __match_proto__(todo_f)
st_kern_bump(struct ocx *ocx, struct todolist *tdl, void *priv)
{
	struct st_bump *stb;

	(void)ocx;
	(void)tdl;
	CAST_OBJ_NOTNULL(stb, priv, ST_BUMP_MAGIC);

	freq0 += stb->bfreq;
	Time_Sim_delta += stb->bphase;
	FREE_OBJ(stb);
	return (TODO_OK);
}

void
Time_Sim_Bump(struct todolist *tdl, double when, double bfreq, double bphase)
{
	struct st_bump *stb;

	ALLOC_OBJ(stb, ST_BUMP_MAGIC);
	AN(stb);
	stb->bfreq = bfreq;
	stb->bphase = bphase;
	(void)TODO_ScheduleRel(tdl, st_kern_bump, stb, when, 0.0, "BUMP");
}

/**********************************************************************/

void
Time_Sim(struct todolist *tdl)
{

	INIT_OBJ(&st_now, TIMESTAMP_MAGIC);
	TS_Add(&st_now, 1e6);
	TB_Now = st_Now;
	TB_Sleep = st_Sleep;
	TB_Step = st_Step;
	TB_Adjust = st_Adjust;
	(void)TODO_ScheduleRel(tdl, st_kern_pll, NULL, 0.0, 1.0, "SIMPLL");
}
