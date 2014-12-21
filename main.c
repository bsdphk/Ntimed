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
 * Main main() functions
 * =====================
 *
 */

#include <string.h>

#include "ntimed.h"
#include "ntp.h"

/*************************************************************************/

static void
dummy(void)
{
	// Reference otherwise unused "library" functions

	NTP_Peer_Destroy(NULL);
}

static int
main_run_tests(int argc, char * const * argv)
{

	(void)argc;
	(void)argv;

	Time_Unix_Passive();

	TS_RunTest(NULL);

	return (0);
}

int
main(int argc, char * const *argv)
{
	if (getpid() == 0)
		dummy();

	if (argc > 1 && !strcmp(argv[1], "--poll-server"))
		return (main_poll_server(argc - 1, argv + 1));
	if (argc > 1 && !strcmp(argv[1], "--sim-client"))
		return (main_sim_client(argc - 1, argv + 1));
	if (argc > 1 && !strcmp(argv[1], "--run-tests"))
		return (main_run_tests(argc - 1, argv + 1));

	return (main_client(argc, argv));
}
