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
 * poll-server
 *	[-d duration]	When to stop
 *	[-m monitor]	Poll this monitor every 32 seconds
 *	[-t tracefile]	Where to save the output (if not stdout)
 *	server ...	What servers to poll
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include <sys/socket.h>

#include "ntimed.h"
#include "ntp.h"
#include "udp.h"

static struct udp_socket *usc;

static void
mps_filter(struct ocx *ocx, const struct ntp_peer *np)
{
	char buf[256];

	NTP_Tool_Format(buf, sizeof buf, np->rx_pkt);
	Put(ocx, OCX_TRACE, "Poll %s %s %s\n", np->hostname, np->ip, buf);
}

static enum todo_e __match_proto__(todo_f)
mps_mon(struct ocx *ocx, struct todolist *tdl, void *priv)
{
	char buf[256];
	struct ntp_peer *np;
	int i;

	(void)ocx;
	(void)tdl;
	CAST_OBJ_NOTNULL(np, priv, NTP_PEER_MAGIC);
	i = NTP_Peer_Poll(ocx, usc, np, 0.2);
	if (i == 1) {
		NTP_Tool_Format(buf, sizeof buf, np->rx_pkt);
		Put(ocx, OCX_TRACE,
		    "Monitor %s %s %s\n", np->hostname, np->ip, buf);
	} else {
		Put(ocx, OCX_TRACE,
		    "Monitor_err %s %s %d\n", np->hostname, np->ip, i);
	}
	return(TODO_OK);
}

static enum todo_e __match_proto__(todo_f)
mps_end(struct ocx *ocx, struct todolist *tdl, void *priv)
{
	(void)tdl;
	(void)priv;
	Put(ocx, OCX_TRACE, "# Run completed\n");
	return(TODO_FAIL);
}

int
main_poll_server(int argc, char *const *argv)
{
	int ch;
	int npeer = 0;
	char *p;
	struct ntp_peerset *npl;
	struct ntp_peer *mon = NULL;
	struct ntp_peer *np;
	struct todolist *tdl;
	double duration = 1800;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	ArgTracefile("-");

	tdl = TODO_NewList();
	Time_Unix_Passive();

	npl = NTP_PeerSet_New(NULL);
	AN(npl);

	while ((ch = getopt(argc, argv, "d:m:t:")) != -1) {
		switch(ch) {
		case 'd':
			duration = strtod(optarg, &p);
			if (*p != '\0' || duration < 1.0)
				Fail(NULL, 0, "Invalid -d argument");
			break;
		case 'm':
			mon = NTP_Peer_NewLookup(NULL, optarg);
			if (mon == NULL)
				Fail(NULL, 0, "Monitor (-m) didn't resolve.");
			break;
		case 't':
			ArgTracefile(optarg);
			break;
		default:
			Fail(NULL, 0,
			    "Usage %s [-d duration] [-m monitor] "
			    "[-t tracefile] server...", argv[0]);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	for (ch = 0; ch < argc; ch++)
		npeer += NTP_PeerSet_Add(NULL, npl, argv[ch]);
	Put(NULL, OCX_TRACE, "# NTIMED Format poll-server 1.0\n");
	Put(NULL, OCX_TRACE, "# Found %d peers\n", npeer);
	if (npeer == 0)
		Fail(NULL, 0, "No peers found");

	NTP_PeerSet_Foreach(np, npl) {
		Put(NULL, OCX_TRACE, "# Peer %s %s\n", np->hostname, np->ip);
		np->filter_func = mps_filter;
	}

	if (mon != NULL)
		Put(NULL, OCX_TRACE,
		    "# Monitor %s %s\n", mon->hostname, mon->ip);

	usc = UdpTimedSocket(NULL);
	assert(usc != NULL);

	TODO_ScheduleRel(tdl, mps_end, NULL, duration, 0, "End task");

	if (mon != NULL)
		TODO_ScheduleRel(tdl, mps_mon, mon, 0, 32, "Monitor");

	NTP_PeerSet_Poll(NULL, npl, usc, tdl);
	(void)TODO_Run(NULL, tdl);
	return (0);
}
