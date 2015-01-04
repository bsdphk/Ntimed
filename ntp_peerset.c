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
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>

#include "ntimed.h"
#include "ntp.h"

struct ntp_group {
	unsigned			magic;
#define NTP_GROUP_MAGIC			0xdd5f58de
	TAILQ_ENTRY(ntp_group)		list;

	char				*hostname;
	int				npeer;
};

struct ntp_peerset {
	unsigned			magic;
#define NTP_PEERSET_MAGIC		0x0bf873d0

	TAILQ_HEAD(,ntp_peer)		head;
	int				npeer;

	TAILQ_HEAD(,ntp_group)		group;
	int				ngroup;

	struct udp_socket		*usc;
	double				t0;
	double				init_duration;
	double				poll_period;
	double				init_packets;
};

/**********************************************************************/

struct ntp_peerset *
NTP_PeerSet_New(struct ocx *ocx)
{
	struct ntp_peerset *npl;

	(void)ocx;
	ALLOC_OBJ(npl, NTP_PEERSET_MAGIC);
	AN(npl);

	TAILQ_INIT(&npl->head);
	TAILQ_INIT(&npl->group);
	return (npl);
}

/**********************************************************************
 * Iterator helpers
 */

struct ntp_peer *
NTP_PeerSet_Iter0(const struct ntp_peerset *npl)
{

	CHECK_OBJ_NOTNULL(npl, NTP_PEERSET_MAGIC);
	return (TAILQ_FIRST(&npl->head));
}

struct ntp_peer *
NTP_PeerSet_IterN(const struct ntp_peerset *npl, const struct ntp_peer *np)
{

	CHECK_OBJ_NOTNULL(npl, NTP_PEERSET_MAGIC);
	CHECK_OBJ_NOTNULL(np, NTP_PEER_MAGIC);
	return (TAILQ_NEXT(np, list));
}

/**********************************************************************/

void
NTP_PeerSet_AddPeer(struct ocx *ocx, struct ntp_peerset *npl,
    struct ntp_peer *np)
{

	(void)ocx;
	CHECK_OBJ_NOTNULL(npl, NTP_PEERSET_MAGIC);
	CHECK_OBJ_NOTNULL(np, NTP_PEER_MAGIC);
	TAILQ_INSERT_TAIL(&npl->head, np, list);
	npl->npeer++;
}

/**********************************************************************/

int
NTP_PeerSet_Add(struct ocx *ocx, struct ntp_peerset *npl,
    const char *hostname)
{
	struct addrinfo hints, *res, *res0;
	int error, n;
	struct ntp_peer *np;
	struct ntp_group *ng;

	CHECK_OBJ_NOTNULL(npl, NTP_PEERSET_MAGIC);
	memset(&hints, 0, sizeof hints);
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	error = getaddrinfo(hostname, "ntp", &hints, &res0);
	if (error)
		Fail(ocx, 1, "hostname '%s', port 'ntp': %s\n",
		    hostname, gai_strerror(error));
	ALLOC_OBJ(ng, NTP_GROUP_MAGIC);
	AN(ng);
	n = 0;
	for (res = res0; res; res = res->ai_next) {
		np = NTP_Peer_New(hostname, res->ai_addr, res->ai_addrlen);
		// XXX: duplicate check
		TAILQ_INSERT_TAIL(&npl->head, np, list);
		npl->npeer++;
		np->group = ng;
		ng->npeer++;
		n++;
	}
	freeaddrinfo(res0);
	if (ng->npeer == 0) {
		FREE_OBJ(ng);
	} else {
		ng->hostname = strdup(hostname);
		AN(ng->hostname);
		TAILQ_INSERT_TAIL(&npl->group, ng, list);
		npl->ngroup++;
	}
	return (n);
}

/**********************************************************************
 * This function is responsible for polling the peers in the set.
 */

static enum todo_e
ntp_peerset_poll(struct ocx *ocx, struct todolist *tdl, void *priv)
{
	struct ntp_peerset *npl;
	struct ntp_peer *np;
	double d, dt;

	(void)ocx;
	CAST_OBJ_NOTNULL(npl, priv, NTP_PEERSET_MAGIC);
	AN(tdl);

	np = TAILQ_FIRST(&npl->head);
	if (np == NULL)
		return(TODO_DONE);

	CHECK_OBJ_NOTNULL(np, NTP_PEER_MAGIC);
	TAILQ_REMOVE(&npl->head, np, list);
	TAILQ_INSERT_TAIL(&npl->head, np, list);

	d = npl->poll_period / npl->npeer;
	if (npl->t0 < npl->init_duration) {
		dt = exp(
		    log(npl->init_duration) / (npl->init_packets * npl->npeer));
		if (npl->t0 * dt < npl->init_duration)
			d = npl->t0 * dt - npl->t0;
	}
	npl->t0 += d;
	TODO_ScheduleRel(tdl, ntp_peerset_poll, npl, d, 0.0, "NTP_PeerSet");
	if (NTP_Peer_Poll(ocx, npl->usc, np, 0.8)) {
		if (np->filter_func != NULL)
			np->filter_func(ocx, np);
	}

	return (TODO_OK);
}

/**********************************************************************
 * This function will be responsible for maintaining the peers in the set
 *
 * XXX: Pick only the best N (3?) servers from any hostname as active.
 * XXX: Re-lookup hostnames on periodic basis to keep the set current
 * XXX: Implement (a future) pool.ntp.org load-balancing protocol
 */

static enum todo_e
ntp_peerset_herd(struct ocx *ocx, struct todolist *tdl, void *priv)
{
	(void)ocx;
	(void)tdl;
	(void)priv;
	return (TODO_DONE);
}

/**********************************************************************/

void
NTP_PeerSet_Poll(struct ocx *ocx, struct ntp_peerset *npl,
    struct udp_socket *usc,
    struct todolist *tdl)
{

	(void)ocx;
	CHECK_OBJ_NOTNULL(npl, NTP_PEERSET_MAGIC);
	AN(usc);
	AN(tdl);

	npl->usc = usc;
	npl->t0 = 1.0;
	npl->init_duration = 64.;
	npl->init_packets = 6.;
	npl->poll_period = 64.;
	TODO_ScheduleRel(tdl, ntp_peerset_poll, npl, 0.0, 0.0,
		"NTP_PeerSet Poll");
	if (npl->ngroup > 0)
		TODO_ScheduleRel(tdl, ntp_peerset_herd, npl,
		    15. * 60. / npl->ngroup, 0.0, "NTP_PeerSet Herd");

}
