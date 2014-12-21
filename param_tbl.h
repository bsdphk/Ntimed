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
 * Parameter Tables
 * ================
 *
 * All parameters tweakable with a -p argument is defined in this file
 * including their minimum, maximum and default values and a not too
 * terse documentation string.
 *
 * Things get instantiated from here by #include param_instance.h
 */

/*lint -save -e525 -e539 */

/* name, min, max, default, docs */

#ifdef PARAM_CLIENT
PARAM_CLIENT(poll_rate, 16.0,	4096.0,	64.0, "")
PARAM_CLIENT(foo, 16.0,	4096.0,	64.0, "")
#endif

#ifdef PARAM_NTP_FILTER

PARAM_NTP_FILTER(ntp_filter_average,
	3, 1e3, 20,
	"Exponential average divisor for average packet delays."
	"  The value chosen is a compromise between gliding through congestion"
	"of common durations and reacting to large-scale routing changes"
	"in a timely manner."
	"  In reality there is no way to tell the two apart."
)

PARAM_NTP_FILTER(ntp_filter_threshold,
	0.01, 10.0, 3.00,
	"Packet delays exceeding the average by this factor are untrustworthy."
	"  Setting this too high increases noise from (mild) congestion."
	"  Setting it too low throws away adequate timestamps."
)

#endif

/**********************************************************************
 * Parameters for pll_std.c
 */

#ifdef PARAM_PLL_STD

PARAM_PLL_STD(pll_std_p_init,
	1e-3, 0.50, 0.33,
	"Proportional term when PLL starts.\n\n"
	"Reducing this will make the PLL more resistant to measurement"
	" noise and jitter, but also makes it converge slower."
	"  Increasing this will allmost certainly cause oscillation."
)

PARAM_PLL_STD(pll_std_i_init,
	10, 1000, 60,
	"Initial P/I ratio when PLL starts.\n\n"
	"Reducing this speed up convergence, but risk overshoot."
	"  Increasing this will slow convergence and reduce impact of noise."
)

PARAM_PLL_STD(pll_std_capture_time,
	20, 1e6, 300,
	"Capture time before stiffning PLL.\n\n"
	"After this many seconds, the PLL will start to stiffen the"
	" P and I terms to gain noise immunity."
	"  Decreasing risks that initial frequency capture is not finished,"
	" which will increase the offset-excursion."
	"  Increasing just delays this stiffning."
)

PARAM_PLL_STD(pll_std_stiffen_rate,
	0.5, 1.0, 0.999,
	"Rate of PLL P/I term stiffning.\n\n"
	"The exponential stiffening per second of D and I terms."
	"  Decreasing makes stiffening faster."
	"  Increasing makes stiffening slower."
)

PARAM_PLL_STD(pll_std_p_limit,
	1e-6, 0.50, 3e-2,
	"Lower limit for Proportional term.\n\n"
	"Reducing this will make the PLL more resistent to noise,"
	" but going to far it will not be able to steer the clock fast enough."
	"  Increasing this makes the PLL more agile and prone to noise."
)

#endif


/*lint -restore */
