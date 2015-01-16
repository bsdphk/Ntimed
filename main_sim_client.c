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
 * sim_client
 *	-s simfile	Output file from poll-server
 *	server_numbers ...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ntimed.h"
#include "ntp.h"

#define PARAM_CLIENT PARAM_INSTANCE
#define PARAM_TABLE_NAME client_param_table
#include "param_instance.h"
#undef PARAM_TABLE_NAME
#undef PARAM_CLIENT

/**********************************************************************/

struct sim_file {
	unsigned		magic;
#define SIM_FILE_MAGIC		0x7f847bd0
	char			*filename;
	FILE			*input;
	unsigned		n_peer;
	struct ntp_peerset	*npl;
	struct timestamp	when;
	unsigned		t0;
};

static void
simfile_poll(struct ocx *ocx, const struct sim_file *sf, char *buf)
{
	char *hostname;
	char *ip;
	char *pkt;
	struct ntp_peer *np;
	struct ntp_packet *rxp;
	struct ntp_packet *txp;

	CHECK_OBJ_NOTNULL(sf, SIM_FILE_MAGIC);
	AN(buf);

	if (memcmp(buf, "Poll ", 5))
		Fail(ocx, 0, "Bad 'Poll' line (%s)\n", buf);
	hostname = buf + 5;
	ip = strchr(hostname, ' ');
	if (ip == NULL)
		Fail(ocx, 0, "Bad 'Poll' line (%s)\n", buf);
	pkt = strchr(ip + 1, ' ');
	if (pkt == NULL)
		Fail(ocx, 0, "Bad 'Poll' line (%s)\n", buf);

	*ip++ = '\0';
	*pkt++ = '\0';

	NTP_PeerSet_Foreach(np, sf->npl)
		if (!strcmp(np->hostname, hostname) && !strcmp(np->ip, ip))
			break;
	if (np == NULL)
		Fail(ocx, 0, "Peer not found (%s, %s)\n", hostname, ip);

	CHECK_OBJ_NOTNULL(np, NTP_PEER_MAGIC);

	txp = np->tx_pkt;
	INIT_OBJ(txp, NTP_PACKET_MAGIC);

	rxp = np->rx_pkt;
	if (NTP_Tool_Scan(rxp, pkt))
		Fail(ocx, 0, "Cannot parse packet (%s, %s, %s)\n",
		    hostname, ip, pkt);

	TS_Add(&rxp->ntp_origin, Time_Sim_delta);
	TS_Add(&rxp->ts_rx, Time_Sim_delta);

	txp->ntp_transmit = rxp->ntp_origin;

	if (np->filter_func != NULL)
		np->filter_func(ocx, np);
}

static enum todo_e
simfile_readline(struct ocx *ocx, struct todolist *tdl, void *priv)
{
	struct sim_file *sf;
	char buf[BUFSIZ], *p;
	struct timestamp t0;
	unsigned u1, u2;
	double dt;

	AN(tdl);
	CAST_OBJ_NOTNULL(sf, priv, SIM_FILE_MAGIC);

	TB_Now(&t0);

	while (1) {
		if (fgets(buf, sizeof buf, sf->input) == NULL) {
			Debug(ocx, "EOF on -s file (%s)\n", sf->filename);
			exit(0);
		}
		p = strchr(buf, '\r');
		if (p != NULL)
			*p = '\0';
		p = strchr(buf, '\n');
		if (p != NULL)
			*p = '\0';

		if (!strncmp(buf, "Now ", 4)) {
			if (sscanf(buf, "Now %u.%u", &u1, &u2) != 2)
				Fail(ocx, 0, "Bad 'Now' line (%s)", buf);
			if (sf->t0 == 0)
				sf->t0 = u1 - t0.sec;
			u1 -= sf->t0;
			TS_Nanosec(&sf->when, u1, u2);
			dt = TS_Diff(&sf->when, &t0);
			if (dt >= 1e-3) {
				TODO_ScheduleAbs(tdl, simfile_readline, priv,
				    &sf->when, 0.0, "Readline");
				return (TODO_OK);
			}
		} else if (!strncmp(buf, "Poll ", 5)) {
			simfile_poll(ocx, sf, buf);
		}
		/* We ignore things we don't understand */
	}
}

static struct sim_file *
SimFile_Open(struct ocx *ocx, const char *fn, struct todolist *tdl,
    struct ntp_peerset *npl)
{
	struct sim_file *sf;
	char buf[BUFSIZ];
	char buf2[BUFSIZ];
	char buf3[BUFSIZ];
	char *e;
	int s;
	unsigned fpeer = 0;

	AN(fn);
	AN(tdl);
	AN(npl);

	ALLOC_OBJ(sf, SIM_FILE_MAGIC);
	AN(sf);

	sf->input = fopen(fn, "r");
	if (sf->input == NULL)
		Fail(ocx, 1, "Could not open -s file (%s)", fn);
	sf->filename = strdup(fn);
	AN(sf->filename);
	sf->npl = npl;

	for (s = 0; s < 3; ) {
		if (fgets(buf, sizeof buf, sf->input) == NULL)
			Fail(ocx, 1, "Premature EOF on -s file (%s)", fn);
		e = strchr(buf, '\0');
		AN(e);
		if (e == buf)
			continue;
		if (e[-1] == '\n')
			*--e = '\0';
		Debug(ocx, ">>> %s\n", buf);
		switch(s) {
		case 0:
			if (strcmp(buf, "# NTIMED Format poll-server 1.0"))
				Fail(ocx, 0,
				    "Wrong fileformat in -s file (%s)", fn);
			s++;
			break;
		case 1:
			if (sscanf(buf, "# Found %u peers", &sf->n_peer) != 1)
				Fail(ocx, 0,
				    "Expected '# Found ... peers' line");
			s++;
			break;
		case 2:
			if (sscanf(buf, "# Peer %s %s", buf2, buf3) != 2)
				Fail(ocx, 0, "Expected '# Peer' line");

			NTP_PeerSet_AddSim(ocx, npl, buf2, buf3);
			if (++fpeer == sf->n_peer)
				s++;
			break;
		default:
			Debug(ocx, "<%s>\n", buf);
			Fail(ocx, 0,
			    "XXX: Wrong state (%d) in open_sim_file", s);
		}
	}
	(void)simfile_readline(NULL, tdl, sf);
	return (sf);
}

int
main_sim_client(int argc, char *const *argv)
{
	int ch;
	const char *s_filename = NULL;
	struct sim_file *sf;
	struct ntp_peerset *npl;
	struct ntp_peer *np;
	struct todolist *tdl;
	struct combine_delta *cd;
	double a, b, c;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	tdl = TODO_NewList();
	Time_Sim(tdl);

	PLL_Init();

	npl = NTP_PeerSet_New(NULL);

	Param_Register(client_param_table);
	NF_Init();

	while ((ch = getopt(argc, argv, "B:s:p:t:")) != -1) {
		switch(ch) {
		case 'B':
			ch = sscanf(optarg, "%lg,%lg,%lg", &a, &b, &c);
			if (ch != 3)
				Fail(NULL, 0,
				    "bad -B argument \"when,freq,phase\"");
			Time_Sim_Bump(tdl, a, b, c);
			break;
		case 's':
			s_filename = optarg;
			break;
		case 'p':
			Param_Tweak(NULL, optarg);
			break;
		case 't':
			ArgTracefile(optarg);
			break;
		default:
			Fail(NULL, 0,
			    "Usage %s [-s simfile] [-p params] [-t tracefile]"
			    " [-B when,freq,phase]", argv[0]);
			break;
		}
	}
	// argc -= optind;
	// argv += optind;

	Param_Report(NULL, OCX_TRACE);

	if (s_filename == NULL)
		Fail(NULL, 1, "You must specify -s file.");

	sf = SimFile_Open(NULL, s_filename, tdl, npl);
	AN(sf);

	cd = CD_New();

	NTP_PeerSet_Foreach(np, npl) {
		NF_New(np);
		np->combiner = CD_AddSource(cd, np->hostname, np->ip);
	}

	(void)TODO_Run(NULL, tdl);

	return (0);
}
