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
 * Standard PLL
 * ============
 *
 * (And the function pointer for accessing any PLL)
 */

#include <math.h>

#include "ntimed.h"

#define PARAM_PLL_STD PARAM_INSTANCE
#define PARAM_TABLE_NAME pll_std_param_table
#include "param_instance.h"
#undef PARAM_TABLE_NAME
#undef PARAM_PLL_STD

static double pll_integrator;
static struct timestamp pll_last_time;
static int pll_mode;
static double pll_a, pll_b;
static struct timestamp pll_t0;
static int pll_generation;

static void __match_proto__(pll_f)
pll_std(struct ocx *ocx, double offset, double weight)
{
	double p_term, dur, dt, rt;
	double used_a, used_b;
	struct timestamp t0;

	TB_Now(&t0);
	p_term = 0.0;
	dur = .0;
	dt = 0;
	used_a = used_b = 0;

	if (pll_generation != TB_generation) {
		pll_mode = 0;
		pll_generation = TB_generation;
	}

	switch (pll_mode) {

	case 0: /* Startup */

		pll_t0 = t0;
		pll_mode = 1;
		pll_a = param_pll_std_p_init;
		pll_b = 0.0;
		break;

	case 1: /* Wait until we have a good estimate, then step */

		rt = TS_Diff(&t0, &pll_t0);
		if (rt > 2.0 && weight > 3) {		// XXX param
			if (fabs(offset) > 1e-3)	// XXX param
				TB_Step(ocx, -offset);
			pll_mode = 2;
			pll_t0 = t0;
		}
		break;

	case 2: /* Wait for another good estimate, then PLL */

		rt = TS_Diff(&t0, &pll_t0);
		if (rt > 6.0) {
			pll_b = pll_a / param_pll_std_i_init;
			pll_t0 = t0;
			pll_mode = 3;
		}
		break;

	case 3: /* track mode */
		rt = TS_Diff(&t0, &pll_t0);
		assert(rt > 0);

		dt = TS_Diff(&t0, &pll_last_time);
		assert(dt > 0);

		/*
		 * XXX: Brute-force exploitation of the weight.
		 *
		 * Ideally, we should scale the pll_[ab] terms and the
		 * stiffening of them based on the weight.  That is harder
		 * than it sounds -- or at least I have not found a good
		 * candidate function yet.
		 * In the meantime this is a simple threshold based
		 * prevention of horribly distant servers injecting too
		 * much noise into the very reactive default PLL.
		 * Some averaging of the weight may be required.
		 */
		if (weight < 50) {
			used_a = 3e-2;
			used_b = 5e-4;
		} else if (weight < 150) {
			used_a = 6e-2;
			used_b = 1e-3;
		} else {

			if (rt > param_pll_std_capture_time &&
			    pll_a > param_pll_std_p_limit) {
				pll_a *= pow(param_pll_std_stiffen_rate, dt);
				pll_b *= pow(param_pll_std_stiffen_rate, dt);
			}
			used_a = pll_a;
			used_b = pll_b;
		}
		p_term = -offset * used_a;
		pll_integrator += p_term * used_b;
		dur = dt;
		break;
	default:
		WRONG("Wrong PLL state");
	}

	dur = ceil(dur);

	/* Clamp (XXX: leave to timebase to do this ?) */
	if (p_term > dur * 500e-6)
		p_term = dur * 500e-6;
	if (p_term < dur * -500e-6)
		p_term = dur * -500e-6;

	pll_last_time = t0;
	Put(ocx, OCX_TRACE,
	    "PLL %d %.3e %.3e %.3e -> %.3e %.3e %.3e %.3e %.3e\n",
	    pll_mode, dt, offset, weight,
	    p_term, dur, pll_integrator,
	    used_a, used_b);
	if (dur > 0.0)
		TB_Adjust(ocx, p_term, dur, pll_integrator);
}

pll_f *PLL = NULL;

void
PLL_Init(void)
{
	Param_Register(pll_std_param_table);
	PLL = pll_std;
}
